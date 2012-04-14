#include "pbvm.h"


// ulong Bitwise_And(ulong, ulong)
DWORD __declspec(dllexport) __stdcall Bitwise_And (vm_state *vm, DWORD arg_count){
	DWORD isnull;
	DWORD a1, a2;
	value v;
	a1=ot_get_ulongarg(vm, &isnull);
	if (isnull) a1=0;
	a2=ot_get_ulongarg(vm, &isnull);
	if (isnull) a2=0;
	v.value=a1&a2;
	v.type=pbvalue_ulong;
	v.flags=0x100;
	ot_set_return_val(vm, &v);
	return 1;
}

DWORD __declspec(dllexport) __stdcall Bitwise_Or (vm_state *vm, DWORD arg_count){
	DWORD isnull;
	DWORD a1, a2;
	value v;
	a1=ot_get_ulongarg(vm, &isnull);
	if (isnull) a1=0;
	a2=ot_get_ulongarg(vm, &isnull);
	if (isnull) a2=0;
	v.value=a1|a2;
	v.type=pbvalue_ulong;
	v.flags=0x100;
	ot_set_return_val(vm, &v);
	return 1;
}

DWORD __declspec(dllexport) __stdcall Bitwise_Xor (vm_state *vm, DWORD arg_count){
	DWORD isnull;
	DWORD a1, a2;
	value v;
	a1=ot_get_ulongarg(vm, &isnull);
	if (isnull) a1=0;
	a2=ot_get_ulongarg(vm, &isnull);
	if (isnull) a2=0;
	v.value=a1^a2;
	v.type=pbvalue_ulong;
	v.flags=0x100;
	ot_set_return_val(vm, &v);
	return 1;
}

DWORD __declspec(dllexport) __stdcall Bitwise_Not (vm_state *vm, DWORD arg_count){
	DWORD isnull;
	DWORD a1;
	value v;
	a1=ot_get_ulongarg(vm, &isnull);
	if (isnull) a1=0;
	v.value=~a1;
	v.type=pbvalue_ulong;
	v.flags=0x100;
	ot_set_return_val(vm, &v);
	return 1;
}

DWORD __declspec(dllexport) __stdcall Bitwise_Shift_Left (vm_state *vm, DWORD arg_count){
	DWORD isnull;
	DWORD a1, a2;
	value v;
	a1=ot_get_ulongarg(vm, &isnull);
	if (isnull) a1=0;
	a2=ot_get_ulongarg(vm, &isnull);
	if (isnull) a2=0;
	v.value=a1<<a2;
	v.type=pbvalue_ulong;
	v.flags=0x100;
	ot_set_return_val(vm, &v);
	return 1;
}

DWORD __declspec(dllexport) __stdcall Bitwise_Shift_Right (vm_state *vm, DWORD arg_count){
	DWORD isnull;
	DWORD a1, a2;
	value v;
	a1=ot_get_ulongarg(vm, &isnull);
	if (isnull) a1=0;
	a2=ot_get_ulongarg(vm, &isnull);
	if (isnull) a2=0;
	v.value=a1>>a2;
	v.type=pbvalue_ulong;
	v.flags=0x100;
	ot_set_return_val(vm, &v);
	return 1;
}

DWORD __declspec(dllexport) __stdcall Address (vm_state *vm, DWORD arg_count){
	value v;

	value *v_value = ot_get_next_evaled_arg_no_convert(vm);
	if (v_value->flags&IS_NULL){
		v.value=0;
	}else if(v_value->type==pbvalue_string){
		v.value=v_value->value;
	}else if(v_value->type==pbvalue_blob){
		v.value=v_value->value+4;
	}
	v.type=pbvalue_ulong;
	v.flags=0x100;
	ot_set_return_val(vm, &v);
	return 1;
}
