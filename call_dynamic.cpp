#include "pbvm.h"

//	any Call_Dynamic( powerobject apow, "function_name", ... / * arguments * / )
//	With this function we can dynamically choose the name of the function to call.

DWORD __declspec(dllexport) __stdcall Call_Dynamic (vm_state *vm, DWORD arg_count){
	value ret;
	LONG _Result;
	value * arguments = GET_EVALEDARGLIST( vm );
	pb_object * objInst = (pb_object*) arguments[0].value;
	//TODO : check for valid instance of objInst
	wchar_t* method = (wchar_t*)arguments[1].value;
	DWORD *vmptr = (DWORD*)vm;
	vmptr[ 0x0204 / 4 ] = arg_count;
	vmptr[ 0x015c / 4 ] = 0;
	vmptr[ 0x0158 / 4 ] = 0;
	vmptr[ 0x0154 / 4 ] = (DWORD)&arguments[2];//skip first and second args
	_Result = ob_invoke_dynamic ( 
					(value*)objInst, 
					NULL,  
					0 ,//FUNCTION 
					method, 
					arg_count - 2, 
					(void*)&arguments[2], 
					&ret );
	//check for exception
	if(GET_THROWNEXCEPTION(vm)){
		DWORD unk_struct_ptr = vmptr[ 0x0160 ];
		*((WORD*)(unk_struct_ptr + 6)) = 8;
		WORD* unk_struct_mbr_ptr = (WORD*)(unk_struct_ptr + 4);
		*unk_struct_mbr_ptr &= 0x0FFFE;
		*unk_struct_mbr_ptr |= 1;
	}
	value * called_return_value = GET_CALLEDRETURNVALUE(vm);
	if(ret.flags == 0x1d01){ //try to know if "ret" is valid or not
		ot_no_return_val(vm);
	}
	else {
		ot_set_return_val(vm, &ret);
	}
    return 1;
}

//TODO: 
//any Call_Dynamic_Event( powerobject apow, "event_name", ... / * arguments * / )
//any Call_Dynamic_Transpose( powerobject apow, "method", any args[] ) : so that arguments can be pushed dynamically in powerscript.
