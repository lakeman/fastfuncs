#include "pbvm.h"
#include <eh.h>

/*
These are the implementations of the n_pb_orca class methods
*/

typedef struct pborca_comperr
{
    INT     iLevel;                         /* Error level */
    wchar_t *   lpszMessageNumber;              /* Pointer to message number */
    wchar_t *   lpszMessageText;                /* Pointer to message text */
    UINT    iColumnNumber;                  /* Column number */
    UINT	iLineNumber;                    /* Line number */
} PBORCA_COMPERR, FAR *PPBORCA_COMPERR;

#define PBCALLBACK(r,n) r ( CALLBACK n )
typedef PBCALLBACK(void, *PBORCA_ERRPROC) ( PPBORCA_COMPERR, LPVOID );

typedef struct{
	vm_state *vm;
	pb_class *obj;
	int refs;
	HMODULE library;
	LPVOID (WINAPI *PBORCA_SessionOpen)			( void );
	VOID  (WINAPI *PBORCA_SessionClose)			( LPVOID hORCASession );
	INT (WINAPI *PBORCA_SessionSetLibraryList)	( LPVOID hORCASession, wchar_t **pLibNames, INT iNumberOfLibs );
	INT (WINAPI *PBORCA_SessionSetCurrentAppl)	( LPVOID hORCASession, wchar_t *lpszApplLibName, wchar_t *lpszApplName );
	VOID (WINAPI *PBORCA_SessionGetError)		( LPVOID hORCASession, wchar_t *buff, int buff_size );
	INT (WINAPI *PBORCA_ApplicationRebuild)		( LPVOID hORCASession, int eRebldType, PBORCA_ERRPROC pCompErrProc, LPVOID pUserData );
	INT (WINAPI *PBORCA_CompileEntryImport)		( LPVOID hORCASession, wchar_t *lpszLibraryName, wchar_t *lpszEntryName, int otEntryType, wchar_t *lpszComments, wchar_t *lpszEntrySyntax, LONG lEntrySyntaxBuffSize, PBORCA_ERRPROC pCompErrProc, LPVOID pUserData );
	INT (WINAPI *PBORCA_CompileEntryRegenerate) ( LPVOID hORCASession, wchar_t *lpszLibraryName, wchar_t *lpszEntryName, int otEntryType, PBORCA_ERRPROC pCompErrProc, LPVOID pUserData );
bool unicode;
}orca_state;

typedef void (*_se_translator_function)(unsigned int, struct _EXCEPTION_POINTERS* );

void SeTrans(unsigned int x, struct _EXCEPTION_POINTERS *p){
	throw p->ExceptionRecord->ExceptionCode;
}

wchar_t* convert_w2a(wchar_t*p){
	size_t bytes=wcstombs(NULL,p,0);
	char *ret=new char[bytes+1];
	wcstombs(ret,p,bytes+1);
	return (wchar_t*)ret;
}

void __stdcall ErrorProc(PPBORCA_COMPERR error, LPVOID *parm){
	orca_state *state=(orca_state *)parm;
	value val;

	if (!state->unicode){
		size_t chars=mbstowcs(NULL,(char*)error->lpszMessageText,0) +1;
		wchar_t*dest = (wchar_t*)pbstg_alc(state->vm, chars *2, GET_HEAP(state->vm));
		mbstowcs(dest,(char*)error->lpszMessageText,chars);
		val.value=(DWORD)dest;
	}else{
		int buff_size=wcslen(error->lpszMessageText)+1;
		wchar_t*dest = (wchar_t*)pbstg_alc(state->vm, buff_size *2, GET_HEAP(state->vm));
		wcscpy(dest,error->lpszMessageText);
		val.value=(DWORD)dest;
	}
	val.type=pbvalue_string;
	val.flags=0x0d00;

	rtRoutineExec(state->vm, 0, state->obj, 0, 0, &val, 1, 21, 1, 0);
	/*TODO
	invoke method? (error->lpszMessageText)
	*/
}

void check_orca_error(vm_state *vm, orca_state *state, LPVOID session, int ret){
	wchar_t buff[256];
	state->PBORCA_SessionGetError(session, buff, 256);
	if (!state->unicode){
		wchar_t buff2[256];
		mbstowcs(buff2,(char*)buff,256);
		Throw_Exception(vm,buff2);
	}else{
		Throw_Exception(vm,buff);
	}
}


DWORD __declspec(dllexport) __stdcall Orca_Destroy (vm_state *vm, DWORD arg_count){
	value field;
	DWORD isvalid;
	pb_class *obj;

	ot_get_curr_obinst_expr(vm, &obj, &isvalid);
	ob_get_field(vm, obj, 1, &field);
	if (field.value){
		orca_state *state=(orca_state*)field.value;
		state->refs --;
		if (state->refs==0){
			if (state->library)
				FreeLibrary(state->library);
			delete state;
		}
	}
	ot_no_return_val(vm);
	return 1;
}

DWORD __declspec(dllexport) __stdcall Orca_Init (vm_state *vm, DWORD arg_count){
	value field;
	DWORD isvalid, isnull;
	orca_state *state;
	pb_class *obj;

	last_vm = vm;

	ot_get_curr_obinst_expr(vm, &obj, &isvalid);
	ob_get_field(vm, obj, 1, &field);
	if (field.value){
		Throw_Exception(vm, L"Orca object already initialised");
		return 1;
	}
	state=new orca_state;
	state->refs=1;
	field.value=(DWORD)state;
	ob_set_field(vm,obj,1,&field);

	wchar_t *dll=(wchar_t *)ot_get_valptr_arg(vm, &isnull);
	if (isnull){
		Throw_Exception(vm, L"Dll name expected");
		return 1;
	}
	state->unicode=(bool)ot_get_simple_intarg(vm, &isnull);
	
	state->library=LoadLibrary(dll);
	if (!state->library){
		Throw_Exception(vm, L"Unable to load orca dll \"%s\"",dll);
		return 1;
	}

	state->PBORCA_SessionOpen				=(LPVOID(WINAPI *)(void))GetProcAddress(state->library,"PBORCA_SessionOpen");
	state->PBORCA_SessionClose				=(void (WINAPI *)(LPVOID))GetProcAddress(state->library,"PBORCA_SessionClose");
	state->PBORCA_SessionSetLibraryList		=(INT (WINAPI *) (LPVOID, wchar_t **, INT))GetProcAddress(state->library,"PBORCA_SessionSetLibraryList");
	state->PBORCA_SessionSetCurrentAppl		=(INT (WINAPI *) (LPVOID, wchar_t *, wchar_t *))GetProcAddress(state->library,"PBORCA_SessionSetCurrentAppl");
	state->PBORCA_SessionGetError			=(void (WINAPI *) (LPVOID, wchar_t *, int))GetProcAddress(state->library,"PBORCA_SessionGetError");
	state->PBORCA_ApplicationRebuild		=(INT (WINAPI *) (LPVOID, int, PBORCA_ERRPROC, LPVOID))GetProcAddress(state->library,"PBORCA_ApplicationRebuild");
	state->PBORCA_CompileEntryImport		=(INT (WINAPI *) (LPVOID, wchar_t *, wchar_t *, int, wchar_t *, wchar_t *, LONG, PBORCA_ERRPROC, LPVOID))GetProcAddress(state->library,"PBORCA_CompileEntryImport");
	state->PBORCA_CompileEntryRegenerate	=(INT (WINAPI *) (LPVOID, wchar_t *, wchar_t *, int, PBORCA_ERRPROC, LPVOID))GetProcAddress(state->library,"PBORCA_CompileEntryRegenerate");
	
	ot_no_return_val(vm);
	return 1;
}

DWORD __declspec(dllexport) __stdcall Orca_Session_Open (vm_state *vm, DWORD arg_count){
	value field, ret;
	DWORD isvalid;
	orca_state *state;
	pb_class *obj, *session;
	
	_se_translator_function old=_set_se_translator(SeTrans);

	try{

		ot_get_curr_obinst_expr(vm, &obj, &isvalid);
		ob_get_field(vm, obj, 1, &field);
		state=(orca_state*)field.value;
		
		lvalue_ref *session_arg = ot_get_next_lvalue_arg(vm, &isvalid);
		ot_create_obinst_at_lval(vm, session_arg, 0, 0);
		session = (pb_class *)get_lvalue(vm, session_arg)->value;
		
		state->refs++;
		ob_set_field(vm, session, 1, &field);

		ob_get_field(vm, session, 2, &field);
		field.value=(DWORD)state->PBORCA_SessionOpen();
		if (!field.value){
			Throw_Exception(vm,L"Session open failed");
			return 1;
		}
		ob_set_field(vm, session, 2, &field);

		ret.value=(DWORD)session;

		ret.type=0x8000u;
		ret.flags=0xD00u;
		ot_set_return_val(vm, &ret);
	}catch(DWORD code){
		Throw_Exception(vm,L"Error %x during of_create_orca_session",code);
	}catch(...){
		Throw_Exception(vm,L"Unknown error during of_create_orca_session");
	}
	_set_se_translator(old);
	return 1;
}

DWORD __declspec(dllexport) __stdcall Orca_Session_Close (vm_state *vm, DWORD arg_count){
	value field;
	DWORD isvalid;
	pb_class *session;
	
	ot_get_curr_obinst_expr(vm, &session, &isvalid);
	ob_get_field(vm, session, 1, &field);
	orca_state *state=(orca_state*)field.value;
	state->refs --;
	ob_get_field(vm, session, 2, &field);
	state->PBORCA_SessionClose((LPVOID)field.value);

	if (state->refs==0){
		if (state->library)
			FreeLibrary(state->library);
		delete state;
	}

	ot_no_return_val(vm);
	return 1;
}

DWORD __declspec(dllexport) __stdcall Orca_Set_Target (vm_state *vm, DWORD arg_count){
	value field;
	DWORD isvalid;
	pb_class *session;

	_se_translator_function old=_set_se_translator(SeTrans);

	try{

		ot_get_curr_obinst_expr(vm, &session, &isvalid);
		ob_get_field(vm, session, 1, &field);
		orca_state *state=(orca_state*)field.value;

		ob_get_field(vm, session, 2, &field);
		
		value *v_value = ot_get_next_evaled_arg_no_convert(vm);
		pb_array *libs=(pb_array *)v_value->value;
		
		int count = ot_array_num_items(vm, libs);
		wchar_t **lib_list = new wchar_t*[count];

		for (long i=0;i<count;i++){
			value *v=ot_array_index(vm, libs, i);
			if (!state->unicode){
				lib_list[i]=convert_w2a((wchar_t*)v->value);
			}else{
				lib_list[i]=(wchar_t*)v->value;
			}
		}

		int ret=state->PBORCA_SessionSetLibraryList((LPVOID)field.value,lib_list,count);
		if (!state->unicode){
			for (long i=0;i<count;i++){
				delete lib_list[i];
			}
		}
		delete lib_list;
		if (ret!=0){
			check_orca_error(vm, state, (LPVOID)field.value, ret);
			return 1;
		}

		wchar_t *appl_lib=(wchar_t *)ot_get_valptr_arg(vm, &isvalid);
		wchar_t *appl=(wchar_t *)ot_get_valptr_arg(vm, &isvalid);
		
		if (!state->unicode){
			appl_lib=convert_w2a(appl_lib);
			appl=convert_w2a(appl);
		}

		ret = state->PBORCA_SessionSetCurrentAppl((LPVOID)field.value,appl_lib,appl);
		if (!state->unicode){
			delete appl_lib;
			delete appl;
		}
		if (ret!=0){
			check_orca_error(vm, state, (LPVOID)field.value, ret);
			return 1;
		}

		ot_no_return_val(vm);
	}catch(DWORD code){
		Throw_Exception(vm,L"Error %x during of_set_target",code);
	}catch(...){
		Throw_Exception(vm,L"Unknown error during of_set_target");
	}
	_set_se_translator(old);
	return 1;
}

DWORD __declspec(dllexport) __stdcall Orca_Import (vm_state *vm, DWORD arg_count){
	value field;
	DWORD isvalid;
	pb_class *session;

	_se_translator_function old=_set_se_translator(SeTrans);

	try{

		ot_get_curr_obinst_expr(vm, &session, &isvalid);
		ob_get_field(vm, session, 1, &field);
		orca_state *state=(orca_state*)field.value;

		ob_get_field(vm, session, 2, &field);
		
		wchar_t *lib=(wchar_t *)ot_get_valptr_arg(vm, &isvalid);
		wchar_t *entry=(wchar_t *)ot_get_valptr_arg(vm, &isvalid);
		short ent_type = ot_get_simple_intarg(vm, &isvalid);
		wchar_t *comment=(wchar_t *)ot_get_valptr_arg(vm, &isvalid);
		wchar_t *syntax=(wchar_t *)ot_get_valptr_arg(vm, &isvalid);
		if (!state->unicode){
			lib=convert_w2a(lib);
			entry=convert_w2a(entry);
			comment=convert_w2a(comment);
			syntax=convert_w2a(syntax);
		}
		int syntax_len=wcslen(syntax)*2;
		
		state->vm=vm;
		state->obj=session;
		int ret = state->PBORCA_CompileEntryImport((LPVOID)field.value,lib,entry,ent_type,comment,syntax,syntax_len,(PBORCA_ERRPROC)ErrorProc,(LPVOID)state);
		if (!state->unicode){
			delete lib;
			delete entry;
			delete comment;
			delete syntax;
		}
		if (ret!=0){
			check_orca_error(vm, state, (LPVOID)field.value, ret);
			return 1;
		}

		ot_no_return_val(vm);
	}catch(DWORD code){
		Throw_Exception(vm,L"Error %x during of_import",code);
	}catch(...){
		Throw_Exception(vm,L"Unknown error during of_import");
	}
	_set_se_translator(old);
	return 1;
}

DWORD __declspec(dllexport) __stdcall Orca_Regenerate (vm_state *vm, DWORD arg_count){
	value field;
	DWORD isvalid;
	pb_class *session;

	ot_get_curr_obinst_expr(vm, &session, &isvalid);
	ob_get_field(vm, session, 1, &field);
	orca_state *state=(orca_state*)field.value;

	ob_get_field(vm, session, 2, &field);
	
	wchar_t *lib=(wchar_t *)ot_get_valptr_arg(vm, &isvalid);
	wchar_t *entry=(wchar_t *)ot_get_valptr_arg(vm, &isvalid);
	short ent_type = ot_get_simple_intarg(vm, &isvalid);
	
	if (!state->unicode){
		lib=convert_w2a(lib);
		entry=convert_w2a(entry);
	}

	state->vm=vm;
	state->obj=session;
	int ret = state->PBORCA_CompileEntryRegenerate((LPVOID)field.value,lib,entry,ent_type,(PBORCA_ERRPROC)ErrorProc,(LPVOID)state);
	if (!state->unicode){
		delete lib;
		delete entry;
	}
	if (ret!=0){
		check_orca_error(vm, state, (LPVOID)field.value, ret);
		return 1;
	}

	ot_no_return_val(vm);
	return 1;
}

DWORD __declspec(dllexport) __stdcall Orca_Rebuild (vm_state *vm, DWORD arg_count){
	value field;
	DWORD isvalid;
	pb_class *session;

	ot_get_curr_obinst_expr(vm, &session, &isvalid);
	ob_get_field(vm, session, 1, &field);
	orca_state *state=(orca_state*)field.value;

	ob_get_field(vm, session, 2, &field);

	short rebuild_type = ot_get_simple_intarg(vm, &isvalid);

	state->vm=vm;
	state->obj=session;
	int ret = state->PBORCA_ApplicationRebuild((LPVOID)field.value, rebuild_type, (PBORCA_ERRPROC)ErrorProc, (LPVOID)state);
	if (ret!=0){
		check_orca_error(vm, state, (LPVOID)field.value, ret);
		return 1;
	}

	ot_no_return_val(vm);
	return 1;
}
/*
DWORD __declspec(dllexport) __stdcall Orca_Create_Exe (vm_state *vm, DWORD arg_count){
	value field;
	DWORD isvalid;
	pb_class *session;

	ot_get_curr_obinst_expr(vm, &session, &isvalid);
	ob_get_field(vm, session, 1, &field);
	orca_state *state=(orca_state*)field.value;

	ob_get_field(vm, session, 2, &field);
	

	int ret = state->PBORCA_SetExeInfo((LPVOID)field.value,);
	if (ret!=0){
		check_orca_error(vm, state, (LPVOID)field.value, ret);
		return 1;
	}
	int ret = state->PBORCA_ExecutableCreate((LPVOID)field.value,);
	if (ret!=0){
		check_orca_error(vm, state, (LPVOID)field.value, ret);
		return 1;
	}

	ot_no_return_val(vm);
	return 1;
}*/