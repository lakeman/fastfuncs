#include "pbvm.h"


// set of temporary values for building the stack string array.
// note that stack_info->caller_line_no is the line number we are going to return to

typedef struct{
	vm_state *vm;
	pb_array *values;
	int index;
	wchar_t *group_name;
	wchar_t *class_name;
	wchar_t *routine_name;
	wchar_t *trace;
}callback_state;

void set_string(vm_state *vm, wchar_t *source, value *val){
	int len=wcslen(source);
	wchar_t *dest = (wchar_t *)pbstg_alc(vm, (len+1)*2, GET_HEAP(vm));
	wcscpy(dest, source);

	if (!(val->flags&IS_NULL))
		ot_free_val_ptr(vm, val);

	val->value=(DWORD)dest;
	val->flags=0x0d00;
	val->type=pbvalue_string;
}

void stack_build_string(callback_state *state, short line_no){
	wchar_t temp[256];
	if (state->index==0 || state->group_name==NULL)
		return;

	wnsprintf(temp, 256, L"%s.%s.%s Line: %d",state->group_name, state->class_name, state->routine_name, line_no);
	if (state->values != NULL){
		value *val=ot_array_index(state->vm, state->values, state->index -1);
		set_string(state->vm, temp, val);
	}
	if (state->trace!=NULL)
		lstrcatW(state->trace, temp);
}

bool __stdcall callback(stack_info *info, void * arg_value){
    callback_state *state=(callback_state *)arg_value;
	
	stack_build_string(state, info->caller_line_no);
	state->index++;

	// note these are pointers to const memory addresses
    state->group_name= ob_get_group_name(state->vm, info->group_id);
    state->class_name = ob_class_name_not_indirect(state->vm, MAKELONG(info->group_id, info->class_id));

	group_data *str_group = ob_group_data_srch(state->vm,info->group_id);
	class_data *str_class = ob_get_class_entry(state->vm, &str_group, info->class_id);
	state->routine_name = ob_event_module_name(state->vm, str_group, str_class, info->routine_id)-1;
   
    return TRUE;
}

DWORD __declspec(dllexport) __stdcall Stack_Trace (vm_state *vm, DWORD arg_count){

	value ret;
	lvalue_ref *lv_values;
	callback_state state;
	DWORD isnull;
	
	last_vm = vm;

	lv_values=ot_get_next_lvalue_arg(vm, &isnull);
	state.values = ot_array_create_unbounded(vm, MAKELONG(-1,pbvalue_string), 0);

	state.vm=vm;
	state.index=0;
	state.trace=NULL;
	state.group_name=NULL;

    ret.value=shlist_traversal(GET_STACKLIST(vm), &state, callback);
	ret.type=7;
	ret.flags=0x0500;

	stack_build_string(&state, -1);
	
	ot_assign_ref_array(vm, lv_values->ptr, state.values, 0, 0);
	ot_set_return_val(vm, &ret);
    return 1;
}

int WINAPI filter(LPEXCEPTION_POINTERS ptrs){
	if (last_vm!=NULL){
		wchar_t buffer[4096];
		callback_state state;
		*buffer=0;
		state.trace = buffer;
		state.values = NULL;
		state.vm = last_vm;
		state.index = 0;
		state.group_name=NULL;

		void *stack_list = GET_STACKLIST(last_vm);
		shlist_traversal(stack_list, &state, callback);
		stack_build_string(&state, -1);
		MessageBoxW(NULL, buffer, L"Unexpected GPF", MB_OK);
	}
	return EXCEPTION_EXECUTE_HANDLER;
}

void Install_Crash_Hook(){
	SetUnhandledExceptionFilter((LPTOP_LEVEL_EXCEPTION_FILTER)filter);
}

void Uninstall_Crash_Hook(){
	SetUnhandledExceptionFilter(NULL);
}