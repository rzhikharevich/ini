#ifndef INI_H
#define INI_H

#include <stddef.h>
#include <stdbool.h>

/*
 *  A non-owning reference to a string part. 
 */
typedef struct {
	const char *base;
	size_t size;
} INIStringSlice;

typedef struct {
	unsigned line, column;
} INILocation;

/*
 *  Parses the input string slice.
 *
 *  handleEntry is called for each parsed entry.
 *
 *  handleError is called for each detected error.
 *
 *  Returns true if no errors occured, false otherwise.
 */
bool INIParse(
	INIStringSlice input,

	void *entryHandlerCtx,
	void (*handleEntry)(
		void *context,
		INIStringSlice section,
		INIStringSlice key,
		INIStringSlice value
	),

	void *errorHandlerCtx,
	void (*handleError)(
		void *context,
		INILocation location,
		const char *msg
	)
);

#endif
