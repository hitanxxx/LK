#ifndef _L_JSON_H_INCLUDED_
#define _L_JSON_H_INCLUDED_

#define JSON_OBJ 	0x0001
#define JSON_ARR	0x0002
#define JSON_STR	0x0003
#define JSON_NUM	0x0004
#define JSON_TRUE	0x0005
#define JSON_FALSE	0x0006
#define JSON_NULL	0x0007

#define JSON_ROOT	0xffff

typedef struct json_t json_t;
typedef struct json_t {
	mem_list_t * list;
	json_t *	parent;

	uint32 		type;
	string_t 	name;
	double 		num;
} json_t;

typedef struct json_content_t {
	char * p;
	char * end;
} json_content_t;

// get
status json_get_child( json_t * parent, uint32 index, json_t ** child );
status json_get_obj_str( json_t * obj, char * str, uint32 length, json_t ** child );
status json_get_obj_bool( json_t * obj, char * str, uint32 length, json_t ** child );
status json_get_obj_num( json_t * obj, char * str, uint32 length, json_t ** child );
status json_get_obj_null( json_t * obj, char * str, uint32 length, json_t ** child );
status json_get_obj_arr( json_t * obj, char * str, uint32 length, json_t ** child );
status json_get_obj_obj( json_t * obj, char * str, uint32 length, json_t ** child );
status json_get_child_by_name( json_t * parent, char * str, uint32 length, json_t ** child );

// add
json_t * json_add_obj( json_t * parent );
json_t * json_add_arr( json_t * parent );
json_t * json_add_true( json_t * parent );
json_t * json_add_false( json_t * parent );
json_t * json_add_null( json_t * parent );
json_t * json_add_str( json_t * parent, char* str, uint32 length );
json_t * json_add_num( json_t * parent, uint32 num );
json_t * json_obj_add_child( json_t * parent, char * str, uint32 length );


status json_decode( json_t ** json, char * p, char * end );
status json_encode( json_t * json, meta_t ** string );
status json_create( json_t ** json );
status json_free( json_t * json );

#endif
