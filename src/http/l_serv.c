#include "lk.h"

static queue_t queue_apis;

status api_perform_info( void * data );
status api_perform_start( void * data );
status api_perform_stop( void * data );
status api_proxy( void * data );
// ---------
serv_api_t serv_elem_arr[] = {
	{ string("/perform_info"),			api_perform_info },
	{ string("/perform_start"),			api_perform_start },
	{ string("/perform_stop"),			api_perform_stop },
	{ string("/proxy"),					api_proxy 		},
	{ string_null,						NULL			}
};

// serv_response_success ----
static status serv_response_success( webser_t * webser )
{
	/*
	{ "status":"success" }
	*/
	string_t str = string("{\"status\":\"success\"}");
	meta_t * t;

	meta_alloc( &t, str.len );
	memcpy( t->last, str.data, str.len );
	t->last += str.len;

	webser_set_mimetype( webser, ".json", l_strlen(".json") );
	webser->re_status = 200;
	webser->filesize = str.len;
	webser_entity_head( webser );
	webser->response_body = t;

	return webser_response( webser->c->write );
}
// serv_response_error ----
static status serv_response_error( webser_t * webser, char * str, uint32 length )
{
	/*
	{"status":"error","resson":"xxx"}
	*/
	string_t str_head = string("{\"status\":\"error\",\"reason\":\"");
	string_t str_tail = string("\"}");
	meta_t * t;
	uint32 all_length;

	all_length = length + str_head.len + str_tail.len;
	meta_alloc( &t, all_length );

	memcpy( t->last, str_head.data, str_head.len );
	t->last += str_head.len;

	memcpy( t->last, str, length );
	t->last += length;

	memcpy( t->last, str_tail.data, str_tail.len );
	t->last += str_tail.len;

	debug_log( "%s --- t [%.*s]", __func__, meta_len(t->pos, t->last), t->pos );

	webser_set_mimetype( webser, ".json", l_strlen(".json") );
	webser->re_status = 200;
	webser->filesize = all_length;
	webser_entity_head( webser );
	webser->response_body = t;

	return webser_response( webser->c->write );
}
// api_proxy -------------------------------
status api_proxy( void * data )
{
	webser_t * webser;
	meta_t *cl;
	json_t * json;
	status rc;

	webser = (webser_t *)data;
	if( webser->request_body && webser->request_body->body_head ) {
		if( webser->request_body->body_head->next ) {
			return serv_response_error( webser, "json too long", l_strlen("json too long") );
		}
		cl = webser->request_body->body_head;
		if( OK != json_decode( &json, cl->pos, cl->last ) ) {
			err_log(  "%s --- json decode error", __func__ );
			return serv_response_error( webser, "json error", l_strlen("json error") );
		}
		rc = upstream_start( (void*)webser, json );
		json_free( json );
		if( rc == ERROR ) {
			err_log("%s --- upstream start failed", __func__ );
			return serv_response_error( webser, "upstream failed", l_strlen("upstream failed") );
		}
		return rc;
	}
	err_log(  "%s --- have't webser body", __func__ );
	return serv_response_error( webser, "json empty", l_strlen("json empty") );
}
// api_perform_info -------------------
status api_perform_info( void * data )
{
	webser_t * webser;
	json_t * json;
	meta_t * t;

	webser = data;
	if( !conf.perf_switch ){
		err_log("%s ---configuration perf off", __func__ );
		return serv_response_error( webser, "configuration perf off", l_strlen("configuration perf off") );
	}
	if( OK != performance_count_output( &json ) ) {
		err_log(  "%s --- put info", __func__ );
		return serv_response_error( webser, "unexpect", l_strlen("unexpect") );
	}
	if( OK != json_encode( json, &t ) ) {
		json_free( json );
		err_log(  "%s --- json encode", __func__ );
		return serv_response_error( webser, "unexpect", l_strlen("unexpect") );
	}
	json_free( json );

	webser_set_mimetype( webser, ".json", l_strlen(".json") );
	webser->re_status = 200;
	webser->filesize = meta_len( t->pos, t->last );
	webser_entity_head( webser );
	webser->response_body = t;

	return webser_response( webser->c->write );
}
// api_perform_setting_data_check ------------
static status api_perform_setting_data_check( json_t * json )
{
	json_t *t, *x, *v;
	uint32 i;
	string_t method;

	json_get_child( json, 1, &t );
	if( t->type != JSON_OBJ ) {
		err_log(  "%s --- root not obj", __func__ );
		return ERROR;
	}
	// check time
	if( OK != json_get_obj_num( t, "time", l_strlen("time"), &x ) ) {
		err_log(  "%s --- element missing 'time'", __func__ );
		return ERROR;
	}
	// check concurrent
	if( OK != json_get_obj_num( t, "concurrent", l_strlen("concurrent"), &x ) ) {
		err_log(  "%s --- element missing 'concurrent'", __func__ );
		return ERROR;
	}
	// check keep alive
	if( OK != json_get_obj_bool( t, "keepalive", l_strlen("keepalive"), &x ) ) {
		err_log(  "%s --- element missing 'keepalive'", __func__ );
		return ERROR;
	}
	// check pipeline array
	if( OK != json_get_obj_arr( t, "pipeline", l_strlen("pipeline"), &x ) ) {
		err_log(  "%s --- element missing 'pipeline'", __func__ );
		return ERROR;
	}
	if( x->list->elem_num < 1 ) {
		err_log(  "%s --- pipeline array empty", __func__ );
		return ERROR;
	}
	for( i = 1; i <= x->list->elem_num; i ++ ) {
		json_get_child( x, i, &t );
		// index part
		if( OK != json_get_obj_num( t, "index", l_strlen("index"), &v ) ) {
			err_log(  "%s --- array missing 'index'", __func__ );
			return ERROR;
		}
		// ip part
		if( OK != json_get_obj_str( t, "ip", l_strlen("ip"), &v ) ) {
			err_log(  "%s --- array missing 'ip'", __func__ );
			return ERROR;
		}
		// port part
		if( OK != json_get_obj_num( t, "port", l_strlen("port"), &v ) ) {
			err_log(  "%s --- array missing 'port", __func__ );
			return ERROR;
		}
		// https part
		if( OK != json_get_obj_bool( t, "https", l_strlen("https"), &v ) ) {
			err_log(  "%s --- array missing 'https'", __func__ );
			return ERROR;
		}
		// host part
		if( OK != json_get_obj_str( t, "host", l_strlen("host"), &v ) ) {
			err_log(  "%s --- array missing 'host'", __func__ );
			return ERROR;
		}
		// method part
		if( OK != json_get_obj_str( t, "method", l_strlen("method"), &v ) ) {
			err_log(  "%s --- array missing 'method'", __func__ );
			return ERROR;
		}
		method.data = v->name.data;
		method.len = v->name.len;
		// uri part
		if( OK != json_get_obj_str( t, "uri", l_strlen("uri"), &v ) ) {
			err_log(  "%s --- array missing 'uri'", __func__ );
			return ERROR;
		}
		// body part
		if( method.len == l_strlen("XXXX") ) {
			if( strncmp( method.data, "POST", method.len ) == 0 ||
			strncmp( method.data, "post", method.len ) ) {
				if( OK != json_get_child_by_name( t, "body",
				l_strlen("body"), &v ) ) {
					err_log("%s --- method post, have't 'body'", __func__ );
					return ERROR;
				}
			}
		}
	}
	return OK;
}
// api_perform_start --------------------
status api_perform_start( void * data )
{
	webser_t * webser;
	meta_t *t, *cl;
	uint32 length = 0;
	uint32 multi = 0;
	json_t * json;
	uint32 i = 0;
	int32 perf_file;
	ssize_t status;

	webser = data;
	// check perf mode on/off
	if( !conf.perf_switch ){
		err_log( "%s --- configuration perf off", __func__ );
		return serv_response_error( webser, "configuration perf off", l_strlen("configuration perf off") );
	}
	// if on, check perform is running/not running
	if( performance_process_running( ) == 1 ) {
		err_log( "%s --- perform working", __func__ );
		return serv_response_error( webser, "perf running", l_strlen("perf running") );
	}
	// perform setting data get
	cl = webser->request_body->body_head;
	while( cl ) {
		multi++;
		length += meta_len( cl->pos, cl->last );
		cl = cl->next;
	}
	if( multi < 1 ) {
		return serv_response_error( webser, "no body", l_strlen("no body") );
	} else {
		meta_alloc( &t, length );
		cl = webser->request_body->body_head;
		while( cl ) {
			memcpy( t->last, cl->pos, meta_len( cl->pos, cl->last ) );
			t->last += meta_len( cl->pos, cl->last );
			cl = cl->next;
		}
	}
	// perform setting data check
	if( OK != json_decode( &json, t->pos, t->last ) ) {
		err_log( "%s --- json decode", __func__ );
		meta_free( t );
		return serv_response_error( webser, "json error", l_strlen("json error") );
	}
	if( OK != api_perform_setting_data_check( json ) ) {
		err_log("%s --- json data check failed", __func__ );
		meta_free( t );
		json_free( json );
		return serv_response_error( webser, "json data error", l_strlen("json data error") );
	}
	json_free( json );
	// perform setting data save
	perf_file = open( L_PATH_PERFTEMP, O_CREAT | O_RDWR | O_TRUNC, 0644 );
	if( perf_file == ERROR ) {
		err_log("%s --- perf data open", __func__ );
		meta_free( t );
		return serv_response_error( webser, "unexpect", l_strlen("unexpect") );;
	}
	status = write( perf_file, t->start, meta_len( t->start, t->end ) );
	if( status == ERROR ) {
		err_log("%s --- write pid to perf_file", __func__ );
		meta_free( t );
		close( perf_file );
		return serv_response_error( webser, "unexpect", l_strlen("unexpect") );;
	}
	close( perf_file );
	meta_free( t );
	// set signal, prepare goto perform test
	sig_perf = 1;
	if( process_id != 0xffff ) {
		for( i = 0; i < process_id; i ++ ) {
			if( ERROR == kill( process_arr[i].pid, SIGUSR1 ) ) {
				err_log( "%s --- send brother perf", __func__, signal );
				return serv_response_error( webser, "unexpect", l_strlen("unexpect") );;
			}
		}
	}
	return serv_response_success( webser );
}
// api_perform_stop -----------
status api_perform_stop( void * data )
{
	webser_t * webser;
	uint32 i;

	webser = data;
	if( !conf.perf_switch ){
		err_log( "%s --- configuration perf off", __func__ );
		return serv_response_error( webser, "configuration perf off", l_strlen("configuration perf off") );
	}
	if( performance_process_running( ) == 0 ) {
		err_log( "%s --- perform not working", __func__ );
		return serv_response_error( webser, "perf not running", l_strlen("perf not running") );
	}
	sig_perf_stop = 1;
	if( process_id != 0xffff ) {
		for( i = 0; i < process_id; i ++ ) {
			if( ERROR == kill( process_arr[i].pid, SIGUSR2 ) ) {
				err_log( "%s --- send brother perf", __func__, signal );
				return serv_response_error( webser, "unexpect", l_strlen("unexpect") );;
			}
		}
	}
	return serv_response_success( webser );
}
// serv_api_find ------------------------
status serv_api_find( string_t * key, serv_api_handler * handler )
{
	queue_t  * p;
	serv_apis_t *apis;

	for( p = queue_head( &queue_apis );
		p != queue_tail( &queue_apis ); p = queue_next( p ) ) {
		apis = l_get_struct( p, serv_apis_t, queue );
		if( apis->api->name.len == key->len ) {
			if( l_find_str( key->data, key->len, apis->api->name.data,
				apis->api->name.len ) ) {
				if( handler ) {
					*handler = apis->api->handler;
				}
				return OK;
			}
		}
	}
	return ERROR;
}
// serv_api_add ---------------------
static status serv_api_add( serv_api_t * api )
{
	serv_apis_t * apis = NULL;

	apis = (serv_apis_t*)malloc( sizeof(serv_apis_t) );
	if( !apis ) {
		err_log("%s --- malloc apis", __func__ );
		return ERROR;
	}
	memset( apis, 0, sizeof(serv_apis_t) );
	apis->api = api;
	queue_insert_tail( &queue_apis, &apis->queue );
	return OK;
}
// serv_api_init ---------------------
static status serv_api_init( void )
{
	uint32 i = 0;

	for( i = 0; serv_elem_arr[i].handler; i ++ ) {
		if( OK != serv_api_add( &serv_elem_arr[i] ) ) {
			err_log("%s --- add api", __func__ );
			return ERROR;
		}
	}
	return OK;
}
// serv_init ------------------------
status serv_init( void )
{
	queue_init( &queue_apis );

	if( OK != serv_api_init( ) ) {
		return ERROR;
	}
	return OK;
}
// serv_end -----------------------
status serv_end( void )
{
	serv_apis_t * apis = NULL;
	queue_t * queue, *next;

	queue = queue_head( &queue_apis );
	while( queue != queue_tail( &queue_apis ) ) {
		next = queue_next( queue );
		queue_remove( queue );
		apis = l_get_struct( queue, serv_apis_t, queue );
		free( apis );
		queue = next;
	}
	return OK;
}
