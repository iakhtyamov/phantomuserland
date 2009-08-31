/**
 *
 * Phantom OS
 *
 * Copyright (C) 2005-2008 Dmitry Zavalishin, dz@dz.ru
 *
 * Kernel ready: yes
 * Preliminary: no
 *
 *
**/

// I believe this is right, but left old code for any case.
// Need test with huge amount of auto vars to check.
#define ABSFROMROOT 1

#include "vm/internal.h"
#include "vm/internal_da.h"
#include "vm/exception.h"

// Ok. All these methods receive ptr to root stack object da,
// which curr_da field points to the active page data area.
// so rootda is root page da, and s is curr page da.


#define   	page_push(v) 		do { s->stack[s->common.free_cell_ptr++] = v; } while(0)
#define 	page_pop() 		s->stack[--(s->common.free_cell_ptr)]
#define		page_top() 		s->stack[(s->common.free_cell_ptr)-1]

#define 	page_is_empty()		(s->common.free_cell_ptr == 0)
#define 	page_is_full() 		(s->common.free_cell_ptr >= s->common.__sSize)

#define    	no_prev() 		pvm_is_null( s->common.prev )
#define 	no_next() 		pvm_is_null( s->common.next )

#define    	set_me(to) 		do { rootda->common.curr = to; rootda->curr_da = s = (void *)&(to.data->da); } while(0)


#define check_underflow()     \
    do {                      \
        if( page_is_empty() ) \
        {                     \
            if( no_prev() ) pvm_exec_throw( "stack underflow" ); \
            set_me( s->common.prev ); \
        }                             \
    } while(0);

#define check_overflow()                             \
    do {                                             \
        if( page_is_full() )                         \
        {                                            \
            if( no_next() )                          \
                {                                    \
                s->common.next = make();             \
                set_next_prev( s->common.curr );     \
                }                                    \
            set_me( s->common.next );                \
        }                                            \
    } while(0);




/**
 *
 * Object stack goes
 *
**/

#define make() pvm_create_ostack_object(s->common.root)
#define set_next_prev(me) pvm_object_da(s->common.next,object_stack)->common.prev = me

void pvm_ostack_push( struct data_area_4_object_stack* rootda, struct pvm_object o )
{
    struct data_area_4_object_stack* s = rootda->curr_da;

    check_overflow();

    /*if( page_is_full() )
    {
        if( no_next() )
        {
            s->common.next = make();
            set_next_prev( s->common.curr );
        }
        set_me( s->common.next );

        { s->common.curr = s->common.next; s->curr_da = (void *)&(to.data->da); }
    }*/


    if( page_is_full() ) panic("opush page full aft mkpage");
    page_push(o);
}

struct pvm_object pvm_ostack_pop( struct data_area_4_object_stack* rootda )
{
    struct data_area_4_object_stack* s = rootda->curr_da;
    check_underflow();
    if( page_is_empty() ) panic("opop page empty");
    return page_pop();
}

struct pvm_object pvm_ostack_top( struct data_area_4_object_stack* rootda )
{
    struct data_area_4_object_stack* s = rootda->curr_da;
    check_underflow();
    return page_top();
}

int pvm_ostack_empty( struct data_area_4_object_stack* rootda )
{
    struct data_area_4_object_stack* s = rootda->curr_da;
    return page_is_empty() && no_prev();
}


void pvm_ostack_abs_set( struct data_area_4_object_stack* rootda, int abs_pos, struct pvm_object val )
{
    struct data_area_4_object_stack* s = rootda->curr_da;
    struct pvm_object c = s->common.root;

    while( abs_pos >= s->common.__sSize )
    {
        //c = data_area(c)->next;
// TODO ERR prev?? not next??
#if ABSFROMROOT
        c = pvm_object_da(c,object_stack)->common.next;
#else
        c = pvm_object_da(c,object_stack)->common.prev;
#endif
        if( pvm_is_null(c) ) pvm_exec_throw( "o abs_set: out of stack" );
        abs_pos -= s->common.__sSize;
    }

    pvm_object_da(c,object_stack)->stack[abs_pos] = val;
}

struct pvm_object pvm_ostack_abs_get( struct data_area_4_object_stack* rootda, int abs_pos )
{
    struct data_area_4_object_stack* s = rootda->curr_da;
    struct pvm_object c = s->common.root;

    while( abs_pos >= s->common.__sSize )
    {
        //c = data_area(c)->next;
// TODO ERR prev?? not next??
#if ABSFROMROOT
        c = pvm_object_da(c,object_stack)->common.next;
#else
        c = pvm_object_da(c,object_stack)->common.prev;
#endif
        if( pvm_is_null(c) ) pvm_exec_throw( "o abs_get: out of stack" );
        abs_pos -= s->common.__sSize;
    }

    return pvm_object_da(c,object_stack)->stack[abs_pos];
}

struct pvm_object  pvm_ostack_pull( struct data_area_4_object_stack* rootda, int depth )
{
    struct data_area_4_object_stack* s = rootda->curr_da;
    // steps up to needed slot from the bottom of the current page
    int displ = s->common.free_cell_ptr - depth - 1;

    struct pvm_object c = s->common.curr;

    while( displ < 0 )
    {
        //c = data_area(c)->prev;
        c = pvm_object_da(c,object_stack)->common.prev;
        if( pvm_is_null(c) ) pvm_exec_throw( "stack pull: underflow" );
        displ += s->common.__sSize;
    }

    return pvm_object_da(c,object_stack)->stack[displ];

}


/**
 *
 * Integer stack goes
 *
**/

#undef make
#undef set_next_prev
#define make() pvm_create_istack_object(s->common.root)
#define set_next_prev(me) pvm_object_da(s->common.next,integer_stack)->common.prev = me


void pvm_istack_push( struct data_area_4_integer_stack* rootda, int o )
{
    struct data_area_4_integer_stack* s = rootda->curr_da;
    check_overflow();
    page_push(o);
}

int pvm_istack_pop( struct data_area_4_integer_stack* rootda )
{
    struct data_area_4_integer_stack* s = rootda->curr_da;
    check_underflow();
    return page_pop();
}

int pvm_istack_top( struct data_area_4_integer_stack* rootda )
{
    struct data_area_4_integer_stack* s = rootda->curr_da;
    check_underflow();
    return page_top();
}

int pvm_istack_empty( struct data_area_4_integer_stack* rootda )
{
    struct data_area_4_integer_stack* s = rootda->curr_da;
    return page_is_empty() && no_prev();
}






void pvm_istack_abs_set( struct data_area_4_integer_stack* rootda, int abs_pos, int val )
{
    struct data_area_4_integer_stack* s = rootda->curr_da;
    struct pvm_object c = s->common.root;

    while( abs_pos >= s->common.__sSize )
    {
        //c = data_area(c)->next;
// TODO ERR prev?? not next??
#if ABSFROMROOT
        c = pvm_object_da(c,integer_stack)->common.next;
#else
        c = pvm_object_da(c,integer_stack)->common.prev;
#endif
        if( pvm_is_null(c) ) pvm_exec_throw( "i abs_set: out of stack" );
        abs_pos -= s->common.__sSize;
    }

    pvm_object_da(c,integer_stack)->stack[abs_pos] = val;
}

int pvm_istack_abs_get( struct data_area_4_integer_stack* rootda, int abs_pos )
{
// TODO ERR curr?? not root?
    struct data_area_4_integer_stack* s = rootda->curr_da;
    struct pvm_object c = s->common.root;

    while( abs_pos >= s->common.__sSize )
    {
        //c = data_area(c)->next;
// TODO ERR prev?? not next??
#if ABSFROMROOT
        c = pvm_object_da(c,integer_stack)->common.next;
#else
        c = pvm_object_da(c,integer_stack)->common.prev;
#endif
        if( pvm_is_null(c) ) pvm_exec_throw( "i abs_get: out of stack" );
        abs_pos -= s->common.__sSize;
    }

    return pvm_object_da(c,integer_stack)->stack[abs_pos];
}



/**
 *
 * Exceptions stack goes
 *
**/

#undef make
#undef set_next_prev
#define make() pvm_create_estack_object(s->common.root)
#define set_next_prev(me) pvm_object_da(s->common.next,exception_stack)->common.prev = me


void pvm_estack_push( struct data_area_4_exception_stack* rootda, struct pvm_exception_handler e )
{
    struct data_area_4_exception_stack* s = rootda->curr_da;
    check_overflow();
    page_push(e);
}


struct pvm_exception_handler pvm_estack_pop( struct data_area_4_exception_stack* rootda )
{
    struct data_area_4_exception_stack* s = rootda->curr_da;
    check_underflow();
    return page_pop();
}

struct pvm_exception_handler pvm_estack_top( struct data_area_4_exception_stack* rootda )
{
    struct data_area_4_exception_stack* s = rootda->curr_da;
    check_underflow();
    return page_top();
}

int pvm_estack_empty( struct data_area_4_exception_stack* rootda )
{
    struct data_area_4_exception_stack* s = rootda->curr_da;
    return page_is_empty() && no_prev();
}


int e_page_foreach( struct data_area_4_exception_stack* s,
                    void *pass,
                    int (*func)( void *pass, struct pvm_exception_handler *elem ) )
{
    int cell = s->common.free_cell_ptr;
//printf("check %d estack cells", cell+1 );
    while( --cell >= 0 )
        if( func( pass, s->stack+cell ) )
            return 1;
    return 0;
}


int pvm_estack_foreach(
                       struct data_area_4_exception_stack* rootda,
                       void *pass,
                       int (*func)( void *pass, struct pvm_exception_handler *elem ))
{
    struct data_area_4_exception_stack* s = rootda->curr_da;
    struct pvm_object c = s->common.curr;
    while( !pvm_is_null( c ) )
    {
        if( e_page_foreach( pvm_data_area(c,exception_stack), pass, func ) )
            return 1;
        //c = data_area(c)->prev;
        c = pvm_object_da(c,exception_stack)->common.prev;
    }
    return 0;
}



/**
 *
 * Stack objects creation.
 * NB. We are deliberately do not use classic init by constructor
 * to speedup creation.
 * TODO: some kind of object pools to recycle stack objects?
 *
 * ssize is number of slots in page
**/

static struct pvm_object     pvm_create_general_stack_object(struct pvm_object object_class, int ssize, int da_size )
{
    struct pvm_object ret = pvm_object_create_dynamic( object_class, da_size );

    // We use only common, so any stack da will do
    struct data_area_4_object_stack* sda = pvm_object_da(ret,object_stack);
    sda->common.root = ret;
    sda->common.curr = ret;
    sda->common.prev.data = 0;
    sda->common.next.data = 0;
    sda->common.free_cell_ptr = 0;
    sda->common.__sSize = ssize;
    sda->curr_da = (void *)sda;

    return ret;
}


struct pvm_object     pvm_create_istack_object()
{
    return pvm_create_general_stack_object(pvm_get_istack_class(), PVM_INTEGER_STACK_SIZE, sizeof(struct data_area_4_integer_stack) );
}

struct pvm_object     pvm_create_ostack_object()
{
    return pvm_create_general_stack_object(pvm_get_ostack_class(), PVM_OBJECT_STACK_SIZE, sizeof(struct data_area_4_object_stack) );
}

struct pvm_object     pvm_create_estack_object()
{
    return pvm_create_general_stack_object(pvm_get_estack_class(), PVM_EXCEPTION_STACK_SIZE, sizeof(struct data_area_4_exception_stack));
}




void pvm_internal_init_ostack(struct pvm_object_storage *os )
{
    struct data_area_4_object_stack* sda = (struct data_area_4_object_stack*) os->da;

    struct pvm_object ret;

    ret.data = os;
    ret.interface = 0; // Nobody needs it there anyway

    sda->common.root = ret;
    sda->common.curr = ret;
    sda->common.prev.data = 0;
    sda->common.next.data = 0;
    sda->common.free_cell_ptr = 0;
    sda->common.__sSize = PVM_OBJECT_STACK_SIZE;
    sda->curr_da = (void *)sda;
}

static inline void gc_fcall( void (*f)(struct pvm_object_storage * o, void *arg), void *arg, pvm_object_t o )
{
	if(o.data == 0) // Don't try to process null objects
		return;
	f( o.data, arg);
	f( o.interface, arg );
}

void pvm_gc_iter_ostack(gc_iterator_call_t func, struct pvm_object_storage * os, void *arg)
{
    struct data_area_4_object_stack *da = (struct data_area_4_object_stack *)&(os->da);
    gc_fcall( func, arg, da->common.root );
    gc_fcall( func, arg, da->common.curr ); // Need?
    gc_fcall( func, arg, da->common.prev );
    gc_fcall( func, arg, da->common.next );

    int i, max = da->common.free_cell_ptr;
    for( i = 0; i < max; i++ )
    {
        gc_fcall( func, arg, da->stack[i] );
    }
}


void pvm_internal_init_istack(struct pvm_object_storage *os )
{
    struct data_area_4_integer_stack* sda = (struct data_area_4_integer_stack*) os->da;

    struct pvm_object ret;

    ret.data = os;
    ret.interface = 0; // Nobody needs it there anyway

    sda->common.root = ret;
    sda->common.curr = ret;
    sda->common.prev.data = 0;
    sda->common.next.data = 0;
    sda->common.free_cell_ptr = 0;
    sda->common.__sSize = PVM_INTEGER_STACK_SIZE;
    sda->curr_da = (void *)sda;
}

void pvm_gc_iter_istack(gc_iterator_call_t func, struct pvm_object_storage * os, void *arg)
{
    // Empty
}


void pvm_internal_init_estack(struct pvm_object_storage *os )
{
    struct data_area_4_exception_stack* sda = (struct data_area_4_exception_stack*) os->da;

    struct pvm_object ret;

    ret.data = os;
    ret.interface = 0; // Nobody needs it there anyway

    sda->common.root = ret;
    sda->common.curr = ret;
    sda->common.prev.data = 0;
    sda->common.next.data = 0;
    sda->common.free_cell_ptr = 0;
    sda->common.__sSize = PVM_EXCEPTION_STACK_SIZE;
    sda->curr_da = (void *)sda;
}


void pvm_gc_iter_estack(gc_iterator_call_t func, struct pvm_object_storage * os, void *arg)
{
    struct data_area_4_exception_stack *da = (struct data_area_4_exception_stack *)&(os->da);
    gc_fcall( func, arg, da->common.root );
    gc_fcall( func, arg, da->common.curr ); // Need?
    gc_fcall( func, arg, da->common.prev );
    gc_fcall( func, arg, da->common.next );

    int i, max = da->common.free_cell_ptr;
    for( i = 0; i < max; i++ )
    {
        gc_fcall( func, arg, da->stack[i].object );
    }
}
