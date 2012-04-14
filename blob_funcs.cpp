#include "pbvm.h"

BOOL copy_val_ptr(vm_state *vm, value *val, blob *b, DWORD &pos, int size, bool field){
	void *dest,*src;

	if (pos+size -1 > b->len) return FALSE;
	
	if (val->flags&IS_NULL||field){
		dest=(void *)pbstg_alc(vm, size, GET_HEAP(vm));
		val->value = (DWORD)dest;
		val->flags = val->flags - IS_NULL;
	}else{
		dest=(void *)val->value;
	}
	src=(void*)(b->data+pos -1);

	memcpy(dest, src, size);
	pos+=size;

	return TRUE;
}

BOOL extract(vm_state *vm, value *val, blob *b, DWORD &pos, bool field){
	if (pos > b->len) return FALSE;

	if (val->flags & IS_ARRAY){
		long count;
		value *v;
		pb_array *parray=(pb_array *)val->value;
		count=ot_array_num_items(vm, parray);
		for (long i=0;i<count;i++){
			v=ot_array_index(vm, parray, i);
			if (!extract(vm, v, b, pos, false)) return FALSE;
		}
		return TRUE;
	}

	if (val->type & 0xC000){
		long fields, first;
		value field;
		pb_class *pclass = (pb_class *)val->value;
		fields = ob_get_no_fields(vm, pclass);
		first = ob_get_first_user_field(vm, pclass);

		for (int i=first;i<fields;i++){
			ob_get_field(vm, pclass, i, &field);
			if (!extract(vm, &field, b, pos, true)) return FALSE;
			if (!(field.flags & IS_ARRAY)){
				ob_set_field(vm, pclass, i, &field);
			}
		}
		return TRUE;
	}
	
	//TODO, ansi / unicode conversions....

	switch(val->type){
		case pbvalue_int:
		case pbvalue_boolean:
		case pbvalue_uint:
		case pbvalue_char:
			{
				if (pos+1 > b->len) return FALSE;

				short *p = (short *)&b->data[pos -1];
				val->value = *p;
				val->flags=0x500;
				pos+=2;
			}
			return TRUE;

		case pbvalue_byte:
			{
				if (pos > b->len) return FALSE;

				char *p = (char *)&b->data[pos -1];
				val->value = *p;
				pos++;
			}
			return TRUE;

		case pbvalue_long:
		case pbvalue_ulong:
			{
				if (pos+3 > b->len) return FALSE;

				long *p = (long *)&b->data[pos -1];
				val->value = *p;
				val->flags=0x1d00;
				pos+=4;
			}
			return TRUE;

		case pbvalue_real:
			{
				if (pos+3 > b->len) return FALSE;

				long *p = (long *)&b->data[pos -1];
				val->value = *p;
				val->flags=0x0900;
				pos+=4;
			}
			return TRUE;
		case pbvalue_string:
			{
				int len;
				wchar_t *dest, *src;

				len=(b->len - pos +1)/2;
				src=(wchar_t*)(b->data+pos -1);
				len=wcsnlen(src, len);
				
				dest=(wchar_t *)pbstg_alc(vm, (len+1) *2, GET_HEAP(vm));

				if ((!field)&&(!(val->flags&IS_NULL)))
					ot_free_val_ptr(vm, val);

				val->value = (DWORD)dest;
				val->flags = 0x0d00;

				wcsncpy(dest, src, len);
				dest[len]=0;
				pos+=(len+1)*2;
			}
			return TRUE;

		case pbvalue_longlong:
		case pbvalue_double:
			return copy_val_ptr(vm, val, b, pos, 8, field);
		case pbvalue_dec:
			return copy_val_ptr(vm, val, b, pos, 16, field);
		case pbvalue_date:
		case pbvalue_time:
		case pbvalue_datetime:
			return copy_val_ptr(vm, val, b, pos, 12, field);
	}
	return FALSE;
}

// boolean blob_extract(ref any value, readonly blob data, ref ulong al_pos)
DWORD __declspec(dllexport) __stdcall Blob_Extract (vm_state *vm, DWORD arg_count){
	DWORD isnull;
	lvalue_ref *lv_val, *lv_pos;
	value ret, *val, *v_pos;
	blob *source;

	lv_val = ot_get_next_lvalue_arg(vm,&isnull);
	val = get_lvalue(vm, lv_val);

	source = (blob *)ot_get_valptr_arg(vm, &isnull);

	lv_pos=ot_get_next_lvalue_arg(vm, &isnull);
	v_pos = get_lvalue(vm, lv_pos);

	ret.value=extract(vm, val, source, v_pos->value, false);

	ret.type=pbvalue_boolean;
	ret.flags=0x500;

	ot_assign_ref_long(vm, lv_pos->ptr, v_pos->value, 0);
	ot_set_return_val(vm, &ret);
	return 1;
}

// PB10 only util function
DWORD __declspec(dllexport) __stdcall Get_Byte_Array (vm_state *vm, DWORD arg_count){
	DWORD start=0, len, isnull;
	blob *source;
	value ret;

	source=(blob *)ot_get_valptr_arg(vm, &isnull);
	len=source->len;
	if (arg_count>=2){
		start=ot_get_ulongarg(vm, &isnull) -1;
		if (start<0) start=0;
		len -= start;
		if (arg_count>=3){
			len=ot_get_ulongarg(vm, &isnull);
			if (len + start > source->len) len=source->len - start;
			if (len<0) len=0;
		}
	}
	
	pb_array *parray=ot_array_create_unbounded(vm, MAKELONG(-1,pbvalue_uint), 0);
	ret.value = (DWORD)parray;
	ret.type = pbvalue_uint;
	ret.flags = IS_ARRAY;
	
	for (DWORD i=0;i<len;i++){
		value *v=ot_array_index(vm, parray, i);
			
		v->value=source->data[start+i] & 0xFF;
		v->flags=0x0d00;
		v->type=pbvalue_uint;
	}
	ot_set_return_val(vm, &ret);
	return 1;
}

void expand_blob(vm_state *vm, value *bv, DWORD len){
	blob *b = (blob*)bv->value;
	if (b->len >= len) return;

	DWORD buff_size = pbstg_sz(vm, b);

	if (len+4 > buff_size){
		buff_size = len+4;
		if (buff_size>6144)
			buff_size|=0xFFF;

		b = (blob *)pbstg_realc(vm, b, buff_size, GET_HEAP(vm));
		bv->value = (DWORD)b;
	}
	b->len = len;
}

BOOL import(vm_state *vm, value *val, value *bv, DWORD &pos){
	if (val->flags&IS_NULL) return FALSE;

	if (val->flags & IS_ARRAY){
		long count;
		value *v;
		pb_array *parray=(pb_array *)val->value;
		count=ot_array_num_items(vm, parray);
		for (long i=0;i<count;i++){
			v=ot_array_index(vm, parray, i);
			if (!import(vm, v, bv, pos)) return FALSE;
		}
		return TRUE;
	}

	if (val->type & 0xC000){
		long fields, first;
		value field;
		pb_class *pclass=(pb_class *)val->value;
		fields = ob_get_no_fields(vm, pclass);
		first = ob_get_first_user_field(vm, pclass);

		for (int i=first;i<fields;i++){
			ob_get_field(vm, pclass, i, &field);
			if (!import(vm, &field, bv, pos)) return FALSE;
		}
		return TRUE;
	}

	int len;
	char *p;

	switch(val->type){
		case pbvalue_int:
		case pbvalue_boolean:
		case pbvalue_uint:
		case pbvalue_char:
			len=2;
			p=(char*)&val->value;
			break;
		case pbvalue_byte:
			len=1;
			p=(char*)&val->value;
			break;
		case pbvalue_long:
		case pbvalue_ulong:
		case pbvalue_real:
			len=4;
			p=(char*)&val->value;
			break;
		case pbvalue_string:
			len = (wcslen((wchar_t *)val->value)+1)*2;
			p=(char*)val->value;
			break;
		case pbvalue_longlong:
		case pbvalue_double:
			len=8;
			p=(char*)val->value;
			break;
		case pbvalue_dec:
			len=16;
			p=(char*)val->value;
			break;
		case pbvalue_date:
		case pbvalue_time:
		case pbvalue_datetime:
			len=12;
			p=(char*)val->value;
			break;
		case pbvalue_blob:
			{
				blob *b = (blob *)val->value;
				len=b->len;
				p=(char*)&b->data;
			}
	}

	expand_blob(vm, bv, pos+len-1);
	blob *b = (blob *)bv->value;

	memcpy(b->data+pos -1, p, len);
	pos+=len;
	return TRUE;
}

// boolean blob_import(readonly any value, ref blob data, ref ulong al_pos)
DWORD __declspec(dllexport) __stdcall Blob_Import (vm_state *vm, DWORD arg_count){
	DWORD isnull;
	DWORD pos;
	value ret;
	lvalue_ref *lv_pos;
	value *v_pos;

	value *v_value = ot_get_next_evaled_arg_no_convert(vm);

	lvalue_ref *lv_data = ot_get_next_lvalue_arg(vm,&isnull);
	value *data = get_lvalue(vm, lv_data);

	if (arg_count==3){
		lv_pos=ot_get_next_lvalue_arg(vm, &isnull);
		v_pos = get_lvalue(vm, lv_pos);
		pos=v_pos->value;
	}else{
		pos=((blob *)data->value)->len +1;
	}

	ret.value=import(vm, v_value, data, pos);
	ret.type=pbvalue_boolean;
	ret.flags=0x500;
	
	if (arg_count==3){
		v_pos->value=pos;
		ot_assign_ref_long(vm, lv_pos->ptr, v_pos->value, 0);
	}
	ot_set_return_val(vm, &ret);
	return 1;
}

DWORD __declspec(dllexport) __stdcall Blob_Mid (vm_state *vm, DWORD arg_count){
	DWORD start, len, isnull;
	blob *source, *dest;
	value v;

	source = (blob *)ot_get_valptr_arg(vm, &isnull);
	if (isnull || source==NULL || source->len==0){
		len =0;
	}else{
		start=ot_get_ulongarg(vm, &isnull);
		if (isnull||start<1) start=1;
		if (start>source->len){
			len=0;
		}else{
			if (arg_count>=3){
				len=ot_get_ulongarg(vm, &isnull);
				if (isnull) len=0;
				if (len>source->len - start+1) len = source->len - start+1;
			}else{
				len=source->len - start+1;
			}
		}
	}

	v.type=pbvalue_blob;
	v.flags=0xd00;
	
	dest = (blob *)pbstg_alc(vm, len +5, GET_HEAP(vm));
	dest->len=len;
	v.value=(DWORD)dest;
	if (len>0)
		memcpy(&dest->data[0], &source->data[start -1], len);
	ot_set_return_val(vm, &v);
	return 1;
}

DWORD __declspec(dllexport) __stdcall Blob_Alloc (vm_state *vm, DWORD arg_count){
	DWORD isnull, new_size;
	blob *dest;
	int zero_memory=FALSE;
	value v;

	new_size=ot_get_ulongarg(vm, &isnull);
	if (isnull) new_size=0;
	
	if (arg_count>=2){
		zero_memory=ot_get_simple_intarg(vm, &isnull);
		if (isnull) zero_memory=FALSE;
	}

	v.type=pbvalue_blob;
	v.flags=0xd00;
	
	dest = (blob *)pbstg_alc(vm, new_size +5, GET_HEAP(vm));
	dest->len=new_size;
	if (zero_memory){
		ZeroMemory(&dest->data[0],new_size);
	}
	v.value=(DWORD)dest;

	ot_set_return_val(vm, &v);
	return 1;
}

