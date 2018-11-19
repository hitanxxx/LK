#include "lk.h"

// meta_file_alloc ------------
status meta_file_alloc( meta_t ** meta, uint32 length )
{
	meta_t * new = NULL;

	new = (meta_t*)l_safe_malloc( sizeof(meta_t) );
	if( !new ) {
		return ERROR;
	}
	memset( new, 0, sizeof(meta_t) );
	new->data = NULL;
	new->next = NULL;

	new->file_flag = 1;
	new->file_pos = 0;
	new->file_last = length;

	*meta = new;
	return OK;
}

// meta_alloc --------------
status meta_alloc( meta_t ** meta, uint32 size )
{
	meta_t * new = NULL;

	new = (meta_t*)l_safe_malloc( sizeof(meta_t) );
	if( !new ) {
		return ERROR;
	}
	memset( new, 0, sizeof(meta_t) );
	new->data = NULL;
	new->next = NULL;

	new->data = (char*)l_safe_malloc( size*sizeof(char) );
	if( !new->data ) {
		l_safe_free( new );
		return ERROR;
	}
	memset( new->data, 0, size*sizeof(char) );
	new->start = new->pos = new->last = new->data;
	new->end = new->start + size*sizeof(char);

	*meta = new;
	return OK;
}
// meta_free ---------------
status meta_free( meta_t * meta )
{
	if( !meta->file_flag ) {
		if( meta->data ) {
			l_safe_free( meta->data );
		}
	}
	l_safe_free( meta );
	meta = NULL;
	return OK;
}
