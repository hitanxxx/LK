#include "lk.h"

static status api_web_response_success( webser_t * webser );
static status api_web_response_failed( webser_t * webser, char * str, uint32 length );

static status api_perform_info( void * data );
static status api_perform_start( void * data );
static status api_perform_stop( void * data );
static status api_proxy( void * data );
static status api_tunnel_setting( void * data );
status api_file_upload( void * data );

// ---------
static serv_api_t api_arr_web[] = {
	{ string("/perform_info"),			api_perform_info },
	{ string("/perform_start"),			api_perform_start },
	{ string("/perform_stop"),			api_perform_stop },
	{ string("/proxy"),					api_proxy },
	{ string("/tunnel_set"),			api_tunnel_setting },
	{ string("/file_upload"),			api_file_upload },
	{ string_null,						NULL			}
};
// api_file_upload ------
status api_file_upload( void * data )
{
	webser_t * webser;
	meta_t * upload_file_meta;
	ssize_t rc;
	l_mem_page_t * page = NULL;
	int32 upload_file = 0;
	char * tips = NULL;

	webser = data;
	// upload limit 1m
	if( webser->request_body->all_length > 1024*1024 ) {
		err_log("%s --- file too big", __func__ );
		tips = "file size limit 1m";
		goto failed;
	}
	if( OK != l_mem_page_create( &page, 4096 ) ) {
		err_log("%s --- mem page create", __func__ );
		tips = "Alloc mem page";
		goto failed;
	}
	if( OK != meta_page_get_all( page, webser->request_body->body_head, &upload_file_meta ) ) {
		err_log("%s --- get body meta failed", __func__ );
		tips = "Get body meta";
		goto failed;
	}
	debug_log("%s --- upload [%.*s]", __func__,
	meta_len( upload_file_meta->pos, upload_file_meta->last ), upload_file_meta->pos );
	upload_file = open( L_PATH_UOLOAD_FILE, O_CREAT|O_RDWR|O_TRUNC, 0644 );
	if( upload_file == ERROR ) {
		err_log("%s --- open file failed", __func__ );
		tips = "Open temp file";
		goto failed;
	}
	rc = write( upload_file, upload_file_meta->pos, meta_len( upload_file_meta->pos, upload_file_meta->last ) );
	if( rc == ERROR ) {
		err_log("%s --- write uoload to file failed", __func__ );
		tips = "Write to temp file";
		goto failed;
	}
	l_mem_page_free( page );
	page = NULL;
	close( upload_file );
	upload_file = 0;
	return api_web_response_success( webser );
failed:
	if( upload_file ) {
		close( upload_file );
	}
	if( page ) {
		l_mem_page_free( page );
	}
	return api_web_response_failed( webser, tips, l_strlen(tips) );
}
// api_tunnel_setting -------------
status api_tunnel_setting( void * data )
{
	webser_t  * webser;
	json_node_t * json;
	status rc;

	webser = data;
	return OK;
}
// api_proxy ---------------
status api_proxy( void * data )
{
	webser_t * webser;
	meta_t * cl;
	status rc = OK;
	json_ctx_t * ctx = NULL;
	l_mem_page_t * page = NULL;
	char * tips = NULL;

	webser = (webser_t *)data;
	if( !webser->request_body || !webser->request_body->body_head ) {
		err_log("%s --- json empty", __func__ );
		tips = "Json empty";
		goto failed;
	}
	if( OK != l_mem_page_create( &page, 4096 ) ) {
		err_log("%s --- mem page create", __func__  );
		tips = "Mem page create";
		goto failed;
	}
	if( OK != meta_page_get_all( page, webser->request_body->body_head, &cl ) ) {
		err_log("%s --- meta page get all", __func__ );
		tips = "Get all meta failed";
		goto failed;
	}
	if( OK != json_ctx_create( &ctx ) ) {
		err_log("%s --- json ctx create", __func__ );
		tips = "Json ctx create";
		goto failed;
	}
	if( OK != json_decode( ctx, cl->pos, cl->last ) ) {
		err_log(  "%s --- json decode error", __func__ );
		tips = "Json decode";
		goto failed;
	}
	rc = upstream_start( (void*)webser, &ctx->root );
	if( rc == ERROR ) {
		err_log("%s --- upstream start failed", __func__ );
		tips = "Upstream failed";
		goto failed;
	}
	l_mem_page_free( page );
	json_ctx_free( ctx );
	return rc;
failed:
	if( page ) {
		l_mem_page_free( page );
	}
	if( ctx ) {
		json_ctx_free( ctx );
	}
	return api_web_response_failed( webser, tips, l_strlen(tips) );
}
// api_perform_info -------------------
status api_perform_info( void * data )
{
	webser_t * webser;
	meta_t * t;
	char * tips = NULL;
	json_ctx_t * ctx = NULL;

	webser = data;
	if( !conf.perf_switch ) {
		err_log("%s --- perf off", __func__ );
		tips = "Perf off";
		goto failed;
	}
	if( OK != json_ctx_create( &ctx ) ) {
		err_log("%s --- json ctx create", __func__ );
		tips = "Json ctx create";
		goto failed;
	}
	if( OK != performance_count_output( (void*)ctx ) ) {
		err_log(  "%s --- performance_count_output", __func__ );
		tips = "perform count output";
		goto failed;
	}
	if( OK != json_encode( ctx, &t ) ) {
		err_log(  "%s --- json encode", __func__ );
		tips = "Json encode";
		goto failed;
	}
	json_ctx_free( ctx );

	webser_set_mimetype( webser, ".json", l_strlen(".json") );
	webser->re_status = 200;
	webser->filesize = meta_len( t->pos, t->last );
	webser_entity_head( webser );
	webser->response_body = t;

	return webser_response( webser->c->write );
failed:
	if( ctx ) {
		json_ctx_free( ctx );
	}
	return api_web_response_failed( webser, tips, l_strlen(tips) );
}
// api_perform_setting_data_check ------------
static status api_perform_setting_data_check( json_node_t * json )
{
	json_node_t *t, *x, *v;
	uint32 i;
	string_t method;
	queue_t * q;

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
	for( q = queue_head( &x->queue ); q != queue_tail( &x->queue ); q = queue_next(q) ) {
		t = l_get_struct( q, json_node_t, queue );
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
		if( OK == l_strncmp_cap( method.data, method.len, "POST", l_strlen("POST") ) ) {
			if( OK != json_get_obj_child_by_name( t, "body", l_strlen("body"), &v ) ) {
				err_log("%s --- method post, have't 'body'", __func__ );
				return ERROR;
			}
		}
	}
	return OK;
}
// api_perform_start --------------------
status api_perform_start( void * data )
{
	webser_t * webser;
	meta_t *cl;
	uint32 i = 0;
	ssize_t status;
	int32 perf_file = 0;
	json_ctx_t * ctx = NULL;
	l_mem_page_t * page = NULL;
	char * tips = NULL;

	webser = data;
	// check perf mode on/off
	if( !conf.perf_switch ) {
		err_log( "%s --- perf off", __func__ );
		tips = "Perf off";
		goto failed;
	}
	// check if perf is running
	if( performance_process_running( ) ) {
		err_log( "%s --- perform is running", __func__ );
		tips = "Perf running";
		goto failed;
	}
	// get request data
	if( !webser->request_body || !webser->request_body->body_head ) {
		err_log("%s --- have't request body", __func__ );
		tips = "data empty";
		goto failed;
	}
	if( OK != l_mem_page_create( &page, 4096 ) ) {
		err_log("%s --- alloc mem page", __func__ );
		tips = "Alloc page";
		goto failed;
	}
	if( OK != meta_page_get_all( page, webser->request_body->body_head, &cl )) {
		err_log("%s --- get body meta", __func__ );
		tips = "Get body";
		goto failed;
	}
	// check request data format
	if( OK != json_ctx_create( &ctx ) ) {
		err_log("%s --- json ctx create", __func__ );
		tips = "json ctx create";
		goto failed;
	}
	if( OK != json_decode( ctx, cl->pos, cl->last ) ) {
		err_log( "%s --- json decode", __func__ );
		tips = "Json decode";
		goto failed;
	}
	// check json format
	if( OK != api_perform_setting_data_check( &ctx->root ) ) {
		err_log("%s --- json data check failed", __func__ );
		tips = "Json data error";
		goto failed;
	}
	json_ctx_free( ctx );
	ctx = NULL;
	// save json data
	perf_file = open( L_PATH_PERFTEMP, O_CREAT|O_RDWR|O_TRUNC, 0644 );
	if( perf_file == ERROR ) {
		err_log("%s --- perf temp file open", __func__ );
		tips = "Perf temp file open failed";
		goto failed;
	}
	status = write( perf_file, cl->pos, meta_len( cl->pos, cl->last ) );
	if( status == ERROR ) {
		err_log("%s --- write pid to perf_file", __func__ );
		tips = "Write to temp file";
		goto failed;
	}
	l_mem_page_free( page );
	page = NULL;
	close( perf_file );
	perf_file = 0;

	// set signal, prepare goto perform test
	sig_perf = 1;
	if( process_id != 0xffff ) {
		// if have worker process
		for( i = 0; i < process_num; i ++ ) {
			if( i == process_id ) continue;
			if( ERROR == kill( process_arr[i].pid, SIGUSR1 ) ) {
				err_log( "%s --- kill perf signal, [%d]", __func__, errno );
				tips = "Kill signal";
				goto failed;
			}
		}
	}
	return api_web_response_success( webser );
failed:
	if( page ) {
		l_mem_page_free( page );
	}
	if( ctx ) {
		json_ctx_free( ctx );
	}
	if( perf_file ) {
		close( perf_file );
	}
	return api_web_response_failed( webser, tips, l_strlen(tips) );
}
// api_perform_stop -----------
status api_perform_stop( void * data )
{
	webser_t * webser;
	uint32 i;
	char * tips = NULL;

	webser = data;
	if( !conf.perf_switch ) {
		err_log( "%s --- perf off", __func__ );
		tips = "Perf off";
		goto failed;
	}
	if( !performance_process_running( ) ) {
		err_log( "%s --- perf not running", __func__ );
		tips = "Perf running";
		goto failed;
	}

	sig_perf_stop = 1;
	if( process_id != 0xffff ) {
		for( i = 0; i < process_num; i ++ ) {
			if( i == process_id ) continue;
			if( ERROR == kill( process_arr[i].pid, SIGUSR2 ) ) {
				err_log( "%s --- kill perf stop signal, [%d]", __func__, signal );
				tips = "Kill signal";
				goto failed;
			}
		}
	}
	return api_web_response_success( webser );
failed:
	return api_web_response_failed( webser, tips, l_strlen(tips) );
}
// api_web_response_success ----
static status api_web_response_success( webser_t * webser )
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
// api_web_response_failed ----
static status api_web_response_failed( webser_t * webser, char * str, uint32 length )
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

	webser_set_mimetype( webser, ".json", l_strlen(".json") );
	webser->re_status = 200;
	webser->filesize = all_length;
	webser_entity_head( webser );
	webser->response_body = t;

	return webser_response( webser->c->write );
}
// webapi_init -----------
status webapi_init( void )
{
    uint32 i;

    for( i = 0; api_arr_web[i].handler; i ++ ) {
		serv_api_register( &api_arr_web[i] );
	}
    return OK;
}
// webapi_end -----------
status webapi_end( void )
{
    return OK;
}
