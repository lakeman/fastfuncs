#include "pbvm.h"

// set of temporary values for building the stack string array.
typedef struct{
	vm_state *vm;
	pb_array *values;
	int index;
	wchar_t *group_name;
	wchar_t *class_name;
	wchar_t *routine_name;
}callback_state;

void stack_build_string(callback_state *state, short line_no){
	wchar_t temp[256];
	value *v=ot_array_index(state->vm, state->values, state->index -1);
	if (!(v->flags&IS_NULL))
		ot_free_val_ptr(state->vm, v);
	
	wnsprintf(temp, 256, L"%s.%s.%s Line: %d",state->group_name, state->class_name, state->routine_name, line_no);
	
	int len=wcslen(temp);
	wchar_t *dest = (wchar_t *)pbstg_alc(state->vm, (len+1)*2, GET_HEAP(state->vm));
	wcscpy(dest,temp);
	v->value=(DWORD)dest;

	v->flags=0x0d00;
	v->type=pbvalue_string;
}

bool __stdcall callback(stack_info *info, void * arg_value){
    callback_state *state=(callback_state *)arg_value;
	
	if (state->index>0){
		stack_build_string(state, info->caller_line_no);
	}
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
	
	lv_values=ot_get_next_lvalue_arg(vm, &isnull);
	state.values = ot_array_create_unbounded(vm, MAKELONG(-1,pbvalue_string), 0);

	state.vm=vm;
	state.index=0;

    ret.value=shlist_traversal(GET_STACKLIST(vm), &state, callback);
	ret.type=7;
	ret.flags=0x0500;

	stack_build_string(&state, 0);
	
	ot_assign_ref_array(vm, lv_values->ptr, state.values, 0, 0);
	ot_set_return_val(vm, &ret);
    return 1;
}