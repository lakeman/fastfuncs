#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <Shlwapi.h>

typedef struct {
	// 0x0004	*this
	// 0x011e	heap ptr
	// 0x015c	stack position
	// 0x0154	stack pointer
	// 0x0158	something else stack related?
}vm_state;

#pragma pack(1)

enum pbvalue_type
{
	pbvalue_notype = 0,
	pbvalue_int,
	pbvalue_long,
	pbvalue_real,
	pbvalue_double,
	pbvalue_dec,
	pbvalue_string,
	pbvalue_boolean,
	pbvalue_any,
	pbvalue_uint,
	pbvalue_ulong,
	pbvalue_blob,
	pbvalue_date,
	pbvalue_time,
	pbvalue_datetime,
	pbvalue_dummy1,
	pbvalue_dummy2,
	pbvalue_dummy3,
	pbvalue_char,
	pbvalue_dummy4,
	pbvalue_longlong,
	pbvalue_byte
};

typedef struct {
	DWORD value;
	short flags;
	/* known flags
		0x0001	is null
		0x0004	autoinstantiate
		0x0040	system type
		0x0100	instance?
		0x0200	shared?
		0x0400	2 byte
		0x0800	not valid?
		0x2000	is array;
	*/
	short type;
}value;

typedef struct {
	DWORD len;
	char data[1];
}blob;

#define IS_NULL 1
#define IS_ARRAY 0x2000

// variable?
typedef struct {
	DWORD flag; // 0 = immediate value / local variable, 1 = object field, 2 = object array element?
	short noidea; // -1??
	short type; 
	short flags; 
	value *value; // +0x0ah
	DWORD parent; // +0x0eh
	DWORD noidea3;
	DWORD item;
}lvalue;

// reference to variable?
typedef struct {
	lvalue *ptr;
	short isnull;
}lvalue_ref;

typedef struct{
    long f1;
    short group_id;//+4
    short class_id;//+6
    short routine_id;
    short f2;
	short f3;
	short f4;
	short f5;
	short f6;
	short f7;
	short f8;
	short f9;
	void * f10;
	short f12;
	short f13;
	short f14;
	short f15;
    short caller_line_no;//+38
	short f16;
	short f17;
	short f18;
	short f19;
	short f20;
	short f21;
	void * f22;
	short f24;
	short f25;
	short f26;
	short f27;
	short f28;
}stack_info;

typedef struct{ // don't need to know what's actually in this struct...
}group_data;

typedef struct{ // don't need to know what's actually in this struct...
}class_data;

typedef struct {
} pb_array;

typedef struct {
} pb_class;

typedef bool __stdcall shlist_callback(stack_info *, void *);

// PBVM imports
value * __stdcall ot_get_field_lv(vm_state *, value *, DWORD);
value * __stdcall ot_get_field_item_lv(vm_state *, value *, DWORD, DWORD);
value * __stdcall ot_get_next_evaled_arg_no_convert(vm_state *);
short __stdcall ot_get_simple_intarg(vm_state *, DWORD *);
int __stdcall ot_array_num_items(vm_state *, pb_array *);
value * __stdcall ot_array_index(vm_state *, pb_array *, int);
void __stdcall ot_set_return_val(vm_state *, value *);
void __stdcall ot_no_return_val(vm_state *);
int __stdcall rt_create_obinst(vm_state *, wchar_t *, pb_class**);
int __stdcall ot_create_obinst_at_lval(vm_state *,lvalue_ref *,int,int);
int __stdcall ob_set_field(vm_state *, int,int,value*);
int __stdcall ob_set_ptr_field(vm_state *, pb_class*,int,void *);
int __stdcall ob_set_ulong_field(vm_state *, int,int,int);
int __stdcall ob_get_ulong_field(vm_state *, int,int);
wchar_t * __stdcall ob_dup_string(vm_state *, wchar_t *);
void * __stdcall ot_get_valptr_arg(vm_state *, DWORD *);
int __stdcall ot_get_curr_obinst_expr(vm_state *, pb_class**, DWORD*);
lvalue_ref * __stdcall ot_get_next_lvalue_arg(vm_state *, DWORD *);
pb_array * __stdcall ot_array_create_unbounded(vm_state *, int, int);
void __stdcall ot_free_val_ptr(vm_state *, value *);
void * __stdcall pbstg_alc(vm_state *, int, int);
void __stdcall ot_assign_ref_array(vm_state *, lvalue *, pb_array*, short, short);
void __stdcall ot_assign_ref_string(vm_state *, lvalue *, wchar_t*, short);
void __stdcall ot_assign_ref_long(vm_state *, lvalue *, int, short);
int __stdcall ob_get_no_fields(vm_state *, pb_class *);
int __stdcall ob_get_first_user_field(vm_state *, pb_class *);
void __stdcall ob_get_field(vm_state *, pb_class *, int, value *);
void __stdcall ob_set_field(vm_state *, pb_class *, int, value *);
int __stdcall ot_get_ulongarg(vm_state *, DWORD *);
int __stdcall pbstg_sz(vm_state *, void *);
void * __stdcall pbstg_realc(vm_state *, void *, int, int);
wchar_t * __stdcall ob_get_group_name(vm_state *, short);
wchar_t * __stdcall ob_class_name_not_indirect(vm_state *, int);
group_data * __stdcall ob_group_data_srch(vm_state *, short);
class_data * __stdcall ob_get_class_entry(vm_state *, group_data **, short);
wchar_t * __stdcall ob_event_module_name(vm_state *, group_data *, class_data *, short);
bool __stdcall shlist_traversal(void *, void *, shlist_callback);
int __stdcall rtRoutineExec(vm_state *, int, pb_class *, int, int, value*, int, int, int, int);

#define GET_HEAP(x) (*(DWORD *)(((char *)x) + 0x11e))
#define GET_STACKLIST(x) (void*)(*(DWORD *)(((char *)x) + 218))
#define GET_THROW(x) (((pb_class**)x)[147])


value * get_lvalue(vm_state *vm, lvalue_ref *value_ref);
void Throw_Exception(vm_state *vm, wchar_t *text, ...);