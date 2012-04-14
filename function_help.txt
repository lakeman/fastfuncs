
Function Documentation
======================


boolean append (ref string as_source, readonly string as_append)
Very fast string append. 
This method appends the second string argument to the end of the source string, while avoiding measuring or duplicating the source string as much as possible.
This method uses the end of the source string buffer to track how long the string is.
Below a buffer size of 6k, PB's normal allocator rounds the buffer size up to the next 2^(N-1) byte boundary (eg ... 8,12,16,24,32,48,64,96 ...).
Above 6k, PB's allocator will give you exactly what you ask for, leaving no room to expand.
When the size of the buffer is above 6k, this append method ensures that the string buffer is expanded in 4k chunks.
WARNING - DO NOT MIX STRING APPENDS WITH OTHER STRING OPERATIONS. 
Always start with an empty string, if a memory buffer with a string length at the end is reused, unexpected results can occur. 
eg;
string ls_x = ''
long ll_i
for ll_i = 1 to 1000000
   append(ls_x, ' ')
next


boolean append (ref blob ab_source, readonly string as_append)
Fast append of strings using a blob variable. This is similar to the above function, but slightly slower and less risky as the length of the blob variable can be used to track the location of the end of the string.
Use string(blob) to extract the string when finished.


unsignedlong bitwise_and (unsignedlong x, unsignedlong y)
unsignedlong bitwise_or (unsignedlong x, unsignedlong y)
Efficient bitwise operations. If you are only interested in the value of one bit, calling mod inline can be faster.


blob blob_alloc (unsignedlong length [ , boolean zero_memory ] )
Allocate a new empty blob without needing to copy anything into it.


boolean blob_extract (ref [any type] ai_val, readonly blob ab_data, ref long al_pos)
Recursively extract typed data from a blob. This behaves in a similar manner to blobedit, but in reverse.
The first reference argument will be populated from the next block of data in the blob starting at al_pos.
On returning, al_pos will have advanced to the next location that data can be read from.
If you pass in a powerobject, all fields will be populated in the order they are defined.
If you pass in an array, all elements will be populated.
Strings will be extracted up to the next NULL character. At this point only unicode strings are supported.
If you want to extract a specific number of characters, pass in an array.

eg you can use this method to very quickly parse and extract an array of structures;

type str_example from structure
	long ll_value1
	datetime ldt_value2
	char lca_value3[50]
end type

str_example lstr_values[10]
blob_extract(lstr_values, lb_data, ll_pos)


boolean blob_import (readonly any aa_value, ref blob ab_data[, ref long al_pos])
This method recursively populates a blob variable from the values passed in.
Exactly the reverse process to blob_extract. If the blob is too small it will be automatically expanded.


blob blob_mid (readonly blob ab_in, unsignedlong al_start [ , unsignedlong al_len ] )
This function behaves exactly like the system blobmid function, but it doesn't take its blob argument by value, so does not force the source blob to be duplicated.


boolean next_tag (readonly string as_source, readonly string as_start_delim, readonly string as_end_delim, ref long al_start_pos, ref string as_tag)
A utility function for parsing some specific text formats.

eg;
string ls_tag
long ll_pos=0

do while next_tag(as_source, '${', '}', ll_pos, ls_tag)
   choose case ls_tag
      case 'example'
         as_source=replace_all(as_source, '${'+ls_tag+'}', 'value')
      case else
         ll_pos+=len(ls_tag)+3
         continue
   end choose
loop


string replace_all (readonly string source, readonly string find, readonly string replace [ , boolean insensitive ] )
A fast method for replacing all instances of a particular string.
This function counts all occurences of the find string, allocates one new string buffer, then copies each segment of the source and replace string into it.
This is *much* faster than any equivalent PB code that must allocate a new string for each replace.


integer sort (ref string as_values[] [ , boolean insensitive [ , ref [any type] aa_sort_too[] ] ] )
Sort an array of strings. If you pass in another array of values of any type, this list will be reordered in the same way.

ag;
string lsa_sortme[]
long ll_i
for ll_i = lowerbound(ana_sort_me) to upperbound(ana_sort_me)
   lsa_sortme[ll_i]=ana_sort_me[ll_i].of_get_sort_string()
next

sort(lsa_sortme, true, ana_sort_me)


boolean split (readonly string as_source, readonly string as_delim, ref string as_values[])
A fast way to split a string into an array.

eg;
string lsa_values[]
split('A,B,C,D', ',', lsa_values)


boolean token (ref string as_source, readonly string as_delim, ref string as_token)
boolean token (readonly string as_source, readonly string as_delim, ref long al_pos, ref string as_token)
Split off one token from a string based on a specific delimiter and return it by reference.
When there are no more tokens the function sets the value to NULL and returns false.
Note, these methods may be convenient but you should consider calling split and looping through the resulting array as it will be more efficient.

eg;
string ls_content, ls_line
string ls_value1, ls_value2, ls_value3
long ll_pos=0

ls_content = filereadex(...)

do while token(ls_content, '~r~n', ll_pos, ls_line)
   token(ls_line,',',ls_value1)
   token(ls_line,',',ls_value2)
   token(ls_line,',',ls_value3)
   ...
loop


long fast_pos (readonly string as_source, readonly string as_find)
long last_pos (readonly string as_source, readonly string as_find)
An experiment to see if I can locate substrings faster than normal PB code.
PB's lastpos reverses the source strings into new buffers before searching.
last_pos is always faster than lastpos and doesn't duplicate the source strings, fast_pos can be faster than pos for some worst cases but will normally be about the same.


subroutine stack_trace (ref string as_call_stack[])
Get the current stack from the VM. This uses a similar process to the existing processes of creating a runtime exception or calling populateerror, but it traverses the entire call stack.
Note that currently the line number of the calling function is incorrect and always zero, but you can work around this by always calling it from a one line wrapping function.

eg;

type n_ex from exception
string isa_stack[]
end type

event constructor;
stack_trace(isa_stack)
end event
