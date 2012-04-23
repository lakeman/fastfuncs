#include "pbvm.h"


value * get_lvalue(vm_state *vm, lvalue_ref *value_ref){
	lvalue *v=value_ref->ptr;
	//if (value_ref->isnull&1) return 0;

	switch(v->flag){
		case 0:// immediate value (local variable?)
			return v->value;
		case 1:// instance field?
			return ot_get_field_lv(vm, v->value, v->parent);
		default:// array element?
			return ot_get_field_item_lv(vm, v->value, v->parent, v->item);
	}
}

void Throw_Exception(vm_state *vm, wchar_t *text, ...){
	pb_class *exception_obj;
	va_list va;
	va_start(va,text);
	rt_create_obinst(vm,L"n_ex",&exception_obj);
	wchar_t x[512];
	wvsprintf(x,text,va);
	ob_set_ptr_field(vm,exception_obj,1,ob_dup_string(vm,x));
	GET_THROW(vm)=exception_obj;
}

void BuildKMPTable(wchar_t *find, int *kmp_table){
	int pos=2, cnd=0;
	
	kmp_table[0] = -1;
	if (!find[0]) return;

	kmp_table[1] = 0;
	if (!find[1]) return;

	while (find[pos]){
		if (find[pos -1] == find[cnd]){
			kmp_table[pos++] = ++cnd;
		}else if(cnd>0){
			if (find[cnd] == find[kmp_table[cnd]])
				cnd = kmp_table[cnd];
			if(cnd>0)
				cnd = kmp_table[cnd];
		}else{
			kmp_table[pos++] = 0;
		}
	}
}

wchar_t * StringSearch(wchar_t *start, wchar_t *find, int *kmp_table){
	wchar_t *p=start;
	long i=0;

	while(*p){
		if (*p == find[i]){
			i++;p++;
			if (!find[i]) return p - i;
		}else if (i>0){
			i=kmp_table[i];
		}else
			p++;
	}
	return NULL;
}

wchar_t * StringSearchi(wchar_t *start, wchar_t *find, int *kmp_table){
	wchar_t *p=start;
	long i=0;

	while(*p){
		if ((iswupper(*p)?towlower(*p):*p)==find[i]){
			i++;p++;
			if (!find[i]) return p - i;
		}else if (i>0){
			i=kmp_table[i];
		}else
			p++;
	}
	return NULL;
}

// boolean split(readonly source, readonly delim, ref string values[])
DWORD __declspec(dllexport) __stdcall Split (vm_state *vm, DWORD arg_count){
	wchar_t *source, *delim;
	DWORD source_null, delim_null, isnull;
	lvalue_ref *lv_values;
	value ret;

	source = (wchar_t *)ot_get_valptr_arg(vm, &source_null);
	delim = (wchar_t *)ot_get_valptr_arg(vm, &delim_null);

	lv_values=ot_get_next_lvalue_arg(vm, &isnull);

	ret.value=FALSE;
	ret.type=7;
	ret.flags=0x0500;
	
	if (source&&*source&&delim&&*delim){
		wchar_t *next, *last=source, *dest;
		DWORD delim_len, index=0, new_size;
		pb_array *values;
		int *kmp_table;

		delim_len=wcslen(delim);

		values = ot_array_create_unbounded(vm, MAKELONG(-1,6), 0);

		kmp_table=new int[delim_len+1];
		BuildKMPTable(delim, kmp_table);

		while(1){
			next=StringSearch(last, delim, kmp_table);
			//next=wcsstr(last, delim);
			if (next==NULL){
				new_size=wcslen(last);
			}else{
				new_size=next - last;
			}
			value *v=ot_array_index(vm, values, index);
			if (!(v->flags&IS_NULL))
				ot_free_val_ptr(vm, v);

			dest = (wchar_t *)pbstg_alc(vm, (new_size+1) *2, GET_HEAP(vm));
			wcsncpy(dest, last, new_size);
			dest[new_size]=0;
			
			v->value=(DWORD)dest;
			v->flags=0x0d00;
			v->type=6;
			if (!next) break;

			index++;
			last=next+delim_len;
		}
		delete kmp_table;

		ot_assign_ref_array(vm, lv_values->ptr, values, 0, 0);
		ret.value=TRUE;
	}
	ot_set_return_val(vm, &ret);
	return 1;
}

// boolean token(ref source, readonly delim, ref string segment)
// calling this repeatedly is quite slow, you should try to use the split method instead, but this method is sometimes useful.
DWORD __declspec(dllexport) __stdcall Token2 (vm_state *vm, DWORD arg_count){
	wchar_t *source, *delim, *next, *dest;
	DWORD isnull, delim_len, new_size;
	lvalue_ref *lv_source, *lv_token;
	value ret, *v_source;
	
	ret.value=TRUE;
	ret.type=7;
	ret.flags=0x0500;
	
	lv_source=ot_get_next_lvalue_arg(vm, &isnull);
	v_source = get_lvalue(vm, lv_source);
	if (!v_source || !v_source->value || !*((wchar_t *)v_source->value)) ret.value=FALSE;

	delim = (wchar_t *)ot_get_valptr_arg(vm, &isnull);
	if (isnull || !delim || !*delim ) ret.value=FALSE;

	lv_token=ot_get_next_lvalue_arg(vm, &isnull);
	
	if (ret.value){
		source = (wchar_t *)v_source->value;

		next=wcsstr(source, delim);

		if (next==NULL){
			new_size=wcslen(source);
			
			dest = (wchar_t *)pbstg_alc(vm, (new_size+1) *2, GET_HEAP(vm));
			wcsncpy(dest, source, new_size);
			dest[new_size]=0;

			ot_assign_ref_string(vm, lv_token->ptr, dest, 0);
			ot_assign_ref_string(vm, lv_source->ptr, NULL, 1);
		}else{
			new_size = (next - source);

			dest = (wchar_t *)pbstg_alc(vm, (new_size+1) *2, GET_HEAP(vm));
			wcsncpy(dest, source, new_size);
			dest[new_size]=0;

			ot_assign_ref_string(vm, lv_token->ptr, dest, 0);

			delim_len=wcslen(delim);
			next+=delim_len;
			new_size = wcslen(next);

			dest = (wchar_t *)pbstg_alc(vm, (new_size+1) *2, GET_HEAP(vm));
			wcsncpy(dest, next, new_size);
			dest[new_size]=0;

			ot_assign_ref_string(vm, lv_source->ptr, dest, 0);
		}
	}
	if (!ret.value){
		ot_assign_ref_string(vm, lv_token->ptr, NULL, 1);
	}
	ot_set_return_val(vm, &ret);
	return 1;}

// boolean token(readonly source, readonly delim, ref long pos, ref string segment)
// calling this repeatedly is quite slow, you should try to use the split method instead
DWORD __declspec(dllexport) __stdcall Token (vm_state *vm, DWORD arg_count){
	wchar_t *source, *delim, *next, *dest;
	DWORD isnull, len, end, pos, delim_len, new_size;
	lvalue_ref *lv_pos, *lv_token;
	value ret, *v_pos;
	
	ret.value=TRUE;
	ret.type=7;
	ret.flags=0x0500;
	
	source = (wchar_t *)ot_get_valptr_arg(vm, &isnull);
	if (isnull || !source || !*source ) ret.value=FALSE;
	delim = (wchar_t *)ot_get_valptr_arg(vm, &isnull);
	if (isnull || !delim || !*delim ) ret.value=FALSE;

	lv_pos=ot_get_next_lvalue_arg(vm, &isnull);
	v_pos = get_lvalue(vm, lv_pos);
	if (!v_pos) ret.value=FALSE;
	
	lv_token=ot_get_next_lvalue_arg(vm, &isnull);
	
	if (ret.value){
		// comments show worked example for; token(" X ","X",pos=1,ret)
		len=wcslen(source);
		// len=3
		pos=v_pos->value;
		if (pos<1)
			pos=1;

		if (pos>len){
			ret.value=FALSE;
		}else{
			delim_len=wcslen(delim);
			// delim_len=1

			next=wcsstr(source + pos -1, delim);
			// next -> " ^X "

			if (next==NULL){
				end=len+1;
			}else{
				end = (next - source);
				// end = 1
			}
			
			ot_assign_ref_long(vm, lv_pos->ptr, end + delim_len + 1, 0);
			// pos = 3

			new_size = end - pos + 1;
			// new_size = 1

			dest = (wchar_t *)pbstg_alc(vm, (new_size+1) *2, GET_HEAP(vm));
			wcsncpy(dest, source + pos -1, new_size);
			dest[new_size]=0;

			ot_assign_ref_string(vm, lv_token->ptr, dest, 0);
		}
	}
	if (!ret.value){
		ot_assign_ref_long(vm, lv_pos->ptr, 1, 0);
		ot_assign_ref_string(vm, lv_token->ptr, NULL, 1);
	}
	ot_set_return_val(vm, &ret);
	return 1;
}

// boolean next_tag(readonly string as_source, readonly string as_start, readonly string as_end, ref long al_start_pos, ref string as_tag)
DWORD __declspec(dllexport) __stdcall Next_Tag (vm_state *vm, DWORD arg_count){
	wchar_t *source, *start_delim, *end_delim;
	DWORD isnull;
	lvalue_ref *lv_pos, *lv_tag;
	value ret, *v_pos;
	
	ret.value=TRUE;
	ret.type=7;
	ret.flags=0x0500;
	
	source = (wchar_t *)ot_get_valptr_arg(vm, &isnull);
	if (isnull || source==NULL || *source==0) ret.value=FALSE;
	start_delim = (wchar_t *)ot_get_valptr_arg(vm, &isnull);
	if (isnull || start_delim==NULL || *start_delim==0) ret.value=FALSE;
	end_delim = (wchar_t *)ot_get_valptr_arg(vm, &isnull);
	if (isnull || end_delim==NULL || *end_delim==0) ret.value=FALSE;
	
	lv_pos=ot_get_next_lvalue_arg(vm, &isnull);
	v_pos = get_lvalue(vm, lv_pos);
	if (!v_pos) ret.value=FALSE;

	lv_tag=ot_get_next_lvalue_arg(vm, &isnull);
	
	if (ret.value){
		DWORD len, pos;

		// comments show worked example for; token(" <X> ","<",">",pos=1,ret)
		len=wcslen(source);
		// len=5
		pos=v_pos->value;
		if (pos<1)
			pos=1;

		if (pos>len){
			ret.value=FALSE;
		}else{
			wchar_t *start, *test, *end, *dest;
			DWORD start_len, end_len, new_size;

			start_len=wcslen(start_delim);
			// start_len=1
			end_len=wcslen(end_delim);
			// end_len=1

			test=source + pos -1;

			while (1){
				// find the next end delimiter
				end=wcsstr(test, end_delim);
				// end -> " <X^> "
				if (end==NULL){
					ret.value=FALSE;
					break;
				}

				// find the last start delimiter before the end delimiter
				start=NULL;
				test=source;
				while(1){
					test = wcsstr(test, start_delim);
					if (test==NULL || test > end) break;
					start=test++;
				}
				
				if (start==NULL){
					// try again, and find the next end delimiter
					test=end + 1;
					continue;
				}
				
				// start -> " ^<X> "
				new_size = end - start - start_len;
				// new_size = 1

				dest = (wchar_t *)pbstg_alc(vm, (new_size+1) *2, GET_HEAP(vm));
				wcsncpy(dest, start + start_len, new_size);
				dest[new_size]=0;
				ot_assign_ref_string(vm, lv_tag->ptr, dest, 0);
				// as_tag="X"

				ot_assign_ref_long(vm, lv_pos->ptr, start - source +1, 0);
				// al_start_pos=2

				break;
			}
		}
	}

	if (!ret.value){
		ot_assign_ref_long(vm, lv_pos->ptr, 1, 0);
		ot_assign_ref_string(vm, lv_tag->ptr, NULL, 1);
	}

	ot_set_return_val(vm, &ret);
	return 1;
}

int __cdecl compare( void *context, const void *arg1, const void *arg2 )
{
   /* Compare all of both strings: */
   return wcscmp( (( wchar_t** )context)[*(long *)arg1], (( wchar_t** )context)[*(long *)arg2]);
}

int __cdecl comparei( void *context, const void *arg1, const void *arg2 )
{
   /* Compare all of both strings: */
   return _wcsicmp( (( wchar_t** )context)[*(long *)arg1], (( wchar_t** )context)[*(long *)arg2]);
}

DWORD __declspec(dllexport) __stdcall Sort (vm_state *vm, DWORD arg_count){
	DWORD ignored;
	value ret;
	long count;
	DWORD insensitive=FALSE;
	lvalue_ref *lv_array1, *lv_array2;
	value *v_array1, *v_array2;
	pb_array *array1, *array2;

	ret.type=1;
	ret.flags=0x500;
	ret.value=1;

	lv_array1 = ot_get_next_lvalue_arg(vm,&ignored);
	v_array1 = get_lvalue(vm, lv_array1);
	if (v_array1){
		array1=(pb_array *)v_array1->value;
		count = ot_array_num_items(vm, array1);
	}else{
		ret.value=-1;
	}

	if (arg_count>=2){
		insensitive = ot_get_simple_intarg(vm, &ignored);
	}

	if (arg_count>=3){
		lv_array2 = ot_get_next_lvalue_arg(vm, &ignored);
		v_array2 = get_lvalue(vm, lv_array2);
		if (v_array2){
			array2=(pb_array *)v_array2->value;
			if (count != ot_array_num_items(vm, array2)){
				ret.value=-1;
			}
		}else{
			ret.value=-1;
		}
	}

	if (ret.value==1){
		
		wchar_t **strings;
		value *values;
		long *index;

		strings=new wchar_t*[count];
		index = new long[count];

		for (long i=0;i<count;i++){
			value *v=ot_array_index(vm, array1, i);
			strings[i]=(wchar_t*)v->value;
			index[i]=i;
		}

		if (arg_count>=3){
			values=new value[count];
			for (long i=0;i<count;i++){
				value *v=ot_array_index(vm, array2, i);
				values[i].value=v->value;
				values[i].flags=v->flags;
				values[i].type=v->type;
			}
		}
		
		qsort_s( index, count, sizeof( long ), (insensitive?comparei:compare), strings );

		for (long i=0;i<count;i++){
			value *v = ot_array_index(vm, array1, i);
			v->value=(DWORD)strings[index[i]];
		}
		if (arg_count>=3){
			for (long i=0;i<count;i++){
				value *v = ot_array_index(vm, array2, i);
				v->value=values[index[i]].value;
				v->flags=values[index[i]].flags;
				v->type=values[index[i]].type;
			}
			delete values;
		}
		delete strings;
		delete index;
	}

	ot_set_return_val(vm, &ret);
	return 1;
}

/*usage:
	long ll_values[]={1,2,3,4,5}
	ll_i = index(4, ll_values)
*/

DWORD __declspec(dllexport) __stdcall Index (vm_state *vm, DWORD arg_count){
	value ret;
	value *v_find = ot_get_next_evaled_arg_no_convert(vm);
	value *v_array = ot_get_next_evaled_arg_no_convert(vm);
	
	int len=0;
	void *p;
	bool ptr = false;

	switch(v_find->type){
		case pbvalue_int:
		case pbvalue_boolean:
		case pbvalue_uint:
		case pbvalue_char:
			len=2;
			break;
		case pbvalue_byte:
			len=1;
			break;
		case pbvalue_long:
		case pbvalue_ulong:
		case pbvalue_real:
			len=4;
			break;
		case pbvalue_longlong:
		case pbvalue_double:
			len=8;
			ptr=true;
			break;
		case pbvalue_dec:
			len=16;
			ptr=true;
			break;
		case pbvalue_blob:
			ptr=true;
			{
				blob *b=(blob*)v_find->value;
				// since the length is at the start, if it doesn't match memcmp won't look any further into potentially unallocated memory
				len = b->len+4; 
			}
			break;
		case pbvalue_string:
			ptr=true;
			break;
		case pbvalue_date:
		case pbvalue_time:
		case pbvalue_datetime:
			len=12;
			ptr=true;
			break;
	}

	if (ptr){
		p=(char*)v_find->value;
	}else{
		p=(char*)&v_find->value;
	}
	ret.value=0;
	ret.type=pbvalue_long;
	ret.flags=0x100;
	
	pb_array *parray=(pb_array *)v_array->value;

	long count = ot_array_num_items(vm, parray);
	for (long i=0;i<count;i++){
		value *v=ot_array_index(vm, parray, i);
		if (v->type == v_find->type){
			if (v_find->flags&IS_NULL){
				if (v->flags&IS_NULL){
					ret.value=i+1;
					break;
				}
			}else if (v->flags&IS_NULL){
				continue;
			}else if (v->type == pbvalue_string){
				if (wcscmp((wchar_t*)p,(wchar_t*)v->value)==0){
					ret.value=i+1;
					break;
				}
			}else if (ptr){
				if (memcmp(p,(void*)v->value,len)==0){
					ret.value=i+1;
					break;
				}
			}else{
				if (memcmp(p,(void*)&v->value,len)==0){
					ret.value=i+1;
					break;
				}
			}
		}
	}
	
	ot_set_return_val(vm, &ret);
	return 1;
}

// in most cases should perform about the same as pos
// doesn't have some of the same worst cases as pos though...
DWORD __declspec(dllexport) __stdcall Fast_Pos (vm_state *vm, DWORD arg_count){
	DWORD source_null, find_null;
	wchar_t *source, *find;
	value v;

	source = (wchar_t *)ot_get_valptr_arg(vm, &source_null);
	find = (wchar_t *)ot_get_valptr_arg(vm, &find_null);
	
	v.value=0;
	v.type=pbvalue_long;
	v.flags=0x100;
	if (!(source_null||find_null)){
		int find_len = wcslen(find);
		if (find_len>0){
			int *kmp_table = new int[find_len];
			BuildKMPTable(find, kmp_table);
			wchar_t *p=StringSearch(source, find, kmp_table);
			delete kmp_table;

			if (p)
				v.value=p - source +1;
		}
	}
	ot_set_return_val(vm, &v);
	return 1;
}

// should perform better than last pos in all cases.
// in some synthetic examples it should be MUCH faster.
DWORD __declspec(dllexport) __stdcall Last_Pos (vm_state *vm, DWORD arg_count){
	DWORD source_null, find_null;
	wchar_t *source, *find;
	value v;

	source = (wchar_t *)ot_get_valptr_arg(vm, &source_null);
	find = (wchar_t *)ot_get_valptr_arg(vm, &find_null);
	
	v.value=0;
	v.type=pbvalue_long;
	v.flags=0x100;
	
	if (!(source_null||find_null)){
		int find_len = wcslen(find);
		
		if (find_len>0){
			int *kmp_table = new int[find_len];
			find_len --;
			int find_pos = find_len;
			int cnd=find_len;
			int pos=cnd;

			kmp_table[pos--]=cnd;
			if (pos>=0){
				kmp_table[pos--]=cnd;
				while (pos>=0){
					if (find[pos+1]==find[cnd]){
						kmp_table[pos--] = --cnd;
					}else if (cnd < find_len){
						if (find[cnd]==find[kmp_table[cnd]])
							cnd=kmp_table[cnd];
						if (cnd < find_len)
							cnd=kmp_table[cnd];
					}else{
						kmp_table[pos--] = find_len;
					}
				}
			}
			wchar_t *p=source+wcslen(source) -1;
			while (p>=source){
				if (*p==find[find_pos]){
					if (find_pos==0){
						v.value=p - source + 1;
						break;
					}
					find_pos --;
					p --;
				}else if (find_pos<find_len){
					find_pos= kmp_table[find_pos];
				}else{
					p --;
				}
			}
			delete kmp_table;
		}
	}
	ot_set_return_val(vm, &v);
	return 1;
}

// string Replace_All (readonly string source, readonly string find, readonly string replace)
DWORD __declspec(dllexport) __stdcall Replace_All (vm_state *vm, DWORD arg_count){
	DWORD source_null, find_null, replace_null, ignored;
	wchar_t *source, *find, *replace;
	DWORD insensitive=FALSE;
	value v;

	source = (wchar_t *)ot_get_valptr_arg(vm, &source_null);
	find = (wchar_t *)ot_get_valptr_arg(vm, &find_null);
	replace = (wchar_t *)ot_get_valptr_arg(vm, &replace_null);
	
	if (arg_count>=4)
		insensitive = ot_get_simple_intarg(vm, &ignored);

	v.value=0;
	v.flags=0x0d01;
	v.type=pbvalue_string;

	if (!(source_null||find_null)){
		long count=0, buff_size, len_find, len_replace=0;
		int *kmp_table;
		wchar_t *p, *dest, *last, *find_copy;

		len_find=wcslen(find);
		
		if (len_find>0){
			kmp_table=new int[len_find+1];
			if (insensitive){
				find_copy=new wchar_t[len_find+1];
				// force the find string to lower case... er...
				for (int i=0;i<len_find+1;i++){
					find_copy[i]=(iswupper(find[i])?towlower(find[i]):find[i]);
				}

				BuildKMPTable(find_copy, kmp_table);
				p=source;
				while ((p=StringSearchi(p, find_copy, kmp_table))!=NULL){
					count++;
					p+=len_find;
				}
			}else{
				BuildKMPTable(find, kmp_table);
				p=source;
				while ((p=StringSearch(p, find, kmp_table))!=NULL){
					count++;
					p+=len_find;
				}
			}
		}

		if (count>0){
			if (replace_null){
				len_replace=0;
			}else{
				len_replace=wcslen(replace);
			}
			buff_size=wcslen(source) + (count * len_replace) - (count * len_find) +1;
		}else{
			buff_size=wcslen(source)+1;
		}

		dest = (wchar_t*)pbstg_alc(vm, buff_size *2, GET_HEAP(vm));
		*dest=0;
		v.value=(DWORD)dest;
		v.flags=0x0d00;

		if (count>0){
			p=source;
			last=p;
			if (insensitive){
				while ((p=StringSearchi(p, find_copy, kmp_table))!=NULL){
					long cpy = (p - last);
					wcsncpy(dest, last, cpy);
					dest+=cpy;
					*dest=0;
					if (len_replace>0){
						wcscpy(dest,replace);
						dest+=len_replace;
					}
					p+=len_find;
					last=p;
				}
				delete find_copy;
			}else{
				while ((p=StringSearch(p, find, kmp_table))!=NULL){
					long cpy = (p - last);
					wcsncpy(dest, last, cpy);
					dest+=cpy;
					*dest=0;
					if (len_replace>0){
						wcscpy(dest,replace);
						dest+=len_replace;
					}
					p+=len_find;
					last=p;
				}
			}
			delete kmp_table;
			wcscpy(dest,last);
		}else{
			wcscpy(dest,source);
		}
	}
	ot_set_return_val(vm, &v);
	return 1;
}

// boolean append(ref string, readonly string)
// boolean append(ref blob, readonly string)

DWORD __declspec(dllexport) __stdcall Append (vm_state *vm, DWORD arg_count){
	DWORD isnull;
	value v;
	
	v.value=TRUE;
	v.type=7;
	v.flags=0x0500;

	lvalue_ref *lv_source=ot_get_next_lvalue_arg(vm, &isnull);
	value *v_source = get_lvalue(vm, lv_source);
	if (v_source->flags&IS_NULL) v.value=FALSE;

	if (v.value){
		int source_len=0;
		wchar_t *dest_ptr=NULL;
		int realloc_size;

		int buff_size = pbstg_sz(vm,(void *)v_source->value);
		const int count = arg_count -1;
		wchar_t **strings = (wchar_t**)_alloca(sizeof(wchar_t**) * count);
		int *string_lengths = (int *)_alloca(sizeof(int) * count);
		int append_len=0;

		for (int i=0;i<count;i++){
			strings[i] = (wchar_t *)ot_get_valptr_arg(vm, &isnull);
			if (isnull || !*strings[i]){
				string_lengths[i]=0;
				continue;
			}
			string_lengths[i] = wcslen(strings[i]);
			append_len+=string_lengths[i]*2;
		}

		if (v_source->type==pbvalue_string){
			dest_ptr = (wchar_t *)v_source->value;
			
			if (*dest_ptr){
				if (buff_size>=32){
					if (*(unsigned short*)(v_source->value + buff_size - 6)==0xABCD){
						source_len = *(int*)(v_source->value + buff_size - 4);
						if (*(unsigned short*)(v_source->value + source_len+2)!=0xABCD)
							source_len=0;
					}
				}

				if (source_len==0){
					source_len = wcslen(dest_ptr)*2;
				}
			}
			realloc_size = source_len + append_len + 10;
		}else{
			source_len = ((blob *)v_source->value)->len+2;
			if (source_len<4 || *(wchar_t*)(v_source->value + source_len))
				source_len+=2;
			realloc_size = source_len + append_len + 2;
		}
		
		if (realloc_size>6144){
			// round up to the nearest 4k
			realloc_size|=0xFFF;
		}
		
		if (realloc_size > buff_size){
			v_source->value = (DWORD)pbstg_realc(vm, (void *)v_source->value, realloc_size, GET_HEAP(vm));
			buff_size = pbstg_sz(vm,(void *)v_source->value);
		}

		dest_ptr = (wchar_t *)(v_source->value + source_len);
		for (int i=0;i<count;i++){
			if (string_lengths[i]==0)
				continue;

			if (strings[i] == dest_ptr){
				// if you're trying to append the source string onto the source string, it may have just moved.
				// (that's also why wcsncpy is used below, or we'd keep appending the string till we hit an invalid page of memory)
				strings[i] = (wchar_t *)v_source->value;
			}
			wcsncpy(dest_ptr, strings[i], string_lengths[i]);
			dest_ptr+=string_lengths[i];
		}

		*dest_ptr=0;

		if (v_source->type==pbvalue_string){
			if (buff_size>=32){
				*(unsigned short*)(v_source->value+source_len+append_len+2)=0xABCD;
				*(unsigned short*)(v_source->value+buff_size-6)=0xABCD;
				*(int*)(v_source->value+buff_size-4)=source_len + append_len;
			}
		}else{
			((blob *)v_source->value)->len = source_len+append_len -2;
		}
	}

	ot_set_return_val(vm, &v);
	return 1;
}


// string value_info(readonly any)
DWORD __declspec(dllexport) __stdcall Value_Info (vm_state *vm, DWORD arg_count){
//	DWORD isnull;
	value v;
	wchar_t info[256];

	value *v_value = ot_get_next_evaled_arg_no_convert(vm);
	
	DWORD len=0;
	DWORD size=0;
	
	if (!(v_value->flags&(IS_NULL|IS_ARRAY))){
		if (v_value->type==pbvalue_string){
			len = (wcslen((wchar_t*)v_value->value)+1)*2;
		}else if (v_value->type==pbvalue_blob){
			len = ((blob*)v_value->value)->len + 4;
		}
		if (len>0)
			size = pbstg_sz(vm,(void *)v_value->value);
	}

	wnsprintf(info, 256, L"Type: %d, Flags: %u, Value: %d, UsedLen: %d, BuffSize %d",v_value->type,v_value->flags, v_value->value, len, size);

	len=wcslen(info);
	wchar_t *dest = (wchar_t *)pbstg_alc(vm, (len+1)*2, GET_HEAP(vm));
	wcscpy(dest,info);
	v.value=(DWORD)dest;
	v.flags=0x0d00;
	v.type=pbvalue_string;

	ot_set_return_val(vm, &v);
	return 1;
}


BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{/*
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}*/
	return TRUE;
}

