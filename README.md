# ini.h

An INI parser.

## Example

	void handleEntry(
		void *context,
		INIStringSlice section,
		INIStringSlice key,
		INIStringSlice value
	) {
		// ...
	}

	void errorHandler(
		void *context,
		INILocation location,
		const char *msg
	) {
		// ...
	}

	INIStringSlice input = {input_str, strlen(input_str)};

	bool ok = INIParse(
		input,
		entryHandlerCtx,
		handleEntry,
		errorHandlerCtx,
		handleError
	);

## Todo

That pile of spaghetti needs a cleanup. INIParse should be split into separate functions.
