#ifndef JML_DEBUG_H
#define JML_DEBUG_H

#ifdef DEBUGGING_OUTPUT
#define DEBUGGING_MESSAGE( format_data... ) fprintf( stderr, format_data )
#else
#define DEBUGGING_MESSAGE( format_data... ) 
#endif

#endif
