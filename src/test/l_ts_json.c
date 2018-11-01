#include "lk.h"

// ---------------------
static void json_test_empty ( void  )
{
	status rc;
	json_t * json;
	string_t string = string("");
	
	rc = json_decode( &json, string.data, string.data + string.len );
	t_assert( rc == ERROR );
}
// ---------------------
static void json_test_num_integer ( void  )
{
	status rc;
	json_t * json, *num;
	string_t string = string(" 1234 ");
	
	rc = json_decode( &json, string.data, string.data + string.len );
	t_assert( rc == OK );
	
	if( rc == OK ) {
		json_get_child( json, 1, &num );
		t_assert( num->type == JSON_NUM );
		t_assert( num->num == 1234 );
		json_free( json );
	}
}
// ---------------------
static void json_test_num_decimal ( void  )
{
	status rc;
	json_t * json, *num;
	string_t string = string(" 0.123 ");
	
	rc = json_decode( &json, string.data, string.data + string.len );
	t_assert( rc == OK );
	
	if( rc == OK ) {
		json_get_child( json, 1, &num );
		t_assert( num->type == JSON_NUM );
		t_assert( num->num == 0.123 );
		json_free( json );
	}
}
// ---------------------
static void json_test_num_minus_integer ( void  )
{
	status rc;
	json_t * json, *num;
	string_t string = string("  -1234 ");
	
	rc = json_decode( &json, string.data, string.data + string.len );
	t_assert( rc == OK );
	
	if( rc == OK ) {
		json_get_child( json, 1, &num );
		t_assert( num->type == JSON_NUM );
		t_assert( num->num == -1234 );
		json_free( json );
	}
}
// ---------------------
static void json_test_num_minus_decimal ( void  )
{
	status rc;
	json_t * json, *num;
	string_t string = string("  -0.123 ");
	
	rc = json_decode( &json, string.data, string.data + string.len );
	t_assert( rc == OK );

	if( rc == OK ) {
		json_get_child( json, 1, &num );
		t_assert( num->type == JSON_NUM );
		t_assert( num->num == -0.123 );
		json_free( json );
	}
}
// ---------------------
static void json_test_true ( void  )
{
	status rc;
	json_t * json, *true;
	string_t string = string("true");
	
	rc = json_decode( &json, string.data, string.data + string.len );
	t_assert( rc == OK );
	
	if( rc == OK ) {
		json_get_child( json, 1, &true );
		t_assert( true->type == JSON_TRUE );
		json_free( json );
	}
}
// --------------------
static void json_test_false ( void  )
{
	status rc;
	json_t * json, *false;
	string_t string = string("false");
	
	rc = json_decode( &json, string.data, string.data + string.len );
	t_assert( rc == OK );
	
	if( rc == OK ) {
		json_get_child( json, 1, &false );
		t_assert( false->type == JSON_FALSE );
		json_free( json );
	}
}
// ---------------------
static void json_test_null ( void  )
{
	status rc;
	json_t * json, *null;
	string_t string = string("null");
	
	rc = json_decode( &json, string.data, string.data + string.len );
	t_assert( rc == OK );
	
	if( rc == OK ) {
		json_get_child( json, 1, &null );
		t_assert( null->type == JSON_NULL );
		json_free( json );
	}
}
// ---------------------
static void json_test_string ( void  )
{
	status rc;
	json_t * json, *str;
	string_t string = string("\"hello world\"");
	
	rc = json_decode( &json, string.data, string.data + string.len );
	t_assert( rc == OK );
	
	if( rc == OK ) {
		json_get_child( json, 1, &str );
		t_assert( str->type == JSON_STR );
		json_free( json ); 
	}
}
// ---------------------
static void json_test_array ( void  )
{
	status rc;
	json_t * json, *arr, *e;
	string_t string = string("[ \"hello\" , true, 1, [] ]");
	
	rc = json_decode( &json, string.data, string.data + string.len );
	t_assert( rc == OK );
	
	if( rc == OK ) {
		json_get_child( json, 1, &arr );
		t_assert( arr->type == JSON_ARR );

		json_get_child( arr, 1, &e );
		t_assert( e->type == JSON_STR );
		t_assert( e->name.len == l_strlen("hello") );
		
		json_get_child( arr, 2, &e );
		t_assert( e->type == JSON_TRUE );
		
		json_get_child( arr, 3, &e );
		t_assert( e->type == JSON_NUM );
		t_assert( e->num == 1 );
		
		json_get_child( arr, 4, &e );
		t_assert( e->type == JSON_ARR );
		
		json_free( json );
	}
}
// ---------------------
static void json_test_obj ( void  )
{
	status rc;
	json_t * json, *obj, *e;
	string_t string = string("{ \"one\" : true, \"two\" : {}, \"three\" : [] }");
	
	rc = json_decode( &json, string.data, string.data + string.len );
	t_assert( rc == OK );
	
	if( rc == OK ) {
		json_get_child( json, 1, &obj );
		t_assert( obj->type == JSON_OBJ );

		json_get_child_by_name( obj, "one", l_strlen("one"), &e );
		t_assert( e->type == JSON_TRUE );
		
		json_get_child_by_name( obj, "two", l_strlen("two"), &e );
		t_assert( e->type == JSON_OBJ );
		
		json_get_child_by_name( obj, "three", l_strlen("three"), &e );
		t_assert( e->type == JSON_ARR );
		
		json_free( json );
	}
}
// ---------------------
static void json_test_obj1 ( void  )
{
	status rc;
	json_t * json, *obj, *e;
	string_t string = string("{ \"one\" : { \"1\" : 123 }, \"two\" : false }");
	
	rc = json_decode( &json, string.data, string.data + string.len );
	t_assert( rc == OK );
	
	if( rc == OK ) {
		json_get_child( json, 1, &obj );
		t_assert( obj->type == JSON_OBJ );

		json_get_child_by_name( obj, "one", l_strlen("one"), &e );
		t_assert( e->type == JSON_OBJ );
		json_get_child_by_name( e, "1", l_strlen("1"), &e );
		t_assert( e->type == JSON_NUM );
		t_assert( e->num == 123 );
		
		json_get_child_by_name( obj, "two", l_strlen("two"), &e );
		t_assert( e->type == JSON_FALSE );
		
		json_free( json );
	}
}
// ---------------------
static void json_test_decode ( void  )
{
	status rc;
	json_t * json;
	string_t string = string("{ \"1\" : [ { \"2\": false }, [ 1, 2, 3 ], true, false, null, 12345, -1 ], \"4\": 555 }");

	rc = json_decode( &json, string.data, string.data + string.len );
	t_assert( rc == OK );
	
	if( rc == OK ) {
		json_free( json );
	}
} 
// ---------------------
static void json_test_encode ( void  )
{
	status rc;
	json_t * json, *e, *parent, *parent1;
	json_t * obj, *arr;
	uint32 i, j;
	
	string_t obj_arr[3] = { string("obj1"), string("obj2"), string("obj3") };
	string_t str_arr[3] = { string("string1"), string("string12"), string("string123") };
	meta_t * meta;
	
	/* {
		"obj1":[
			"string1",
			"string12",
			"string123"
		],
		"obj2":[
			"string1",
			"string12",
			"string123"
		],
		"obj3":[
			"string1",
			"string12",
			"string123"
		]
	} */
	
	rc = json_create( &json );
	t_assert( rc == OK );
	e = mem_list_push( json->list );
	e->type = JSON_OBJ;
	mem_list_create( &e->list, sizeof(json_t) );
	
	parent = e;
	for( i = 0; i < 3; i++ ) {
		
		e = mem_list_push( parent->list );
		e->name.data = obj_arr[i].data;
		e->name.len = obj_arr[i].len;
		mem_list_create( &e->list, sizeof(json_t) );
		
		e = mem_list_push( e->list );
		e->type = JSON_ARR;
		mem_list_create( &e->list, sizeof(json_t) );
		
		parent1 = e;
		for( j = 0; j < 3; j ++ ) {
		
			e = mem_list_push( parent1->list );
			e->name.data = str_arr[j].data;
			e->name.len = str_arr[j].len;
			e->type = JSON_STR;
		}
	}
	rc = json_encode( json, &meta );
	t_assert( rc == OK );
	
	
	if( rc == OK ) {
		json_free( json );
	}
	
	rc = json_decode( &json, meta->pos, meta->last );
	t_assert( rc == OK );
	
	if( rc == OK ) {
		json_get_child( json, 1, &obj );
		t_assert( obj->type == JSON_OBJ );
		
		json_get_child_by_name( obj, "obj1", l_strlen("obj1"), &arr );
		t_assert( arr->type == JSON_ARR );
		
		json_get_child( arr, 1, &e );
		t_assert( e->type == JSON_STR );
		t_assert( e->name.len == l_strlen("string1") );
		
		json_free( json );
	}
	meta_free( meta );
}

// ---------------------
void ts_json_init( )
{
	test_add( json_test_empty );
	
	test_add( json_test_num_integer );
	test_add( json_test_num_decimal );
	test_add( json_test_num_minus_integer );
	test_add( json_test_num_minus_decimal );
	
	test_add( json_test_true );
	test_add( json_test_false );
	test_add( json_test_null );
	
	test_add( json_test_string );
	
	test_add( json_test_array );
	
	test_add( json_test_obj );
	test_add( json_test_obj1 );
	
	test_add( json_test_decode );
	
	test_add( json_test_encode );
}