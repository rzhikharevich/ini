#include "ini.h"

#ifdef __GNUC__
#define CountLeadingZeros32(x) __builtin_clz(x)
#elif defined(_MSC_VER)
#include <intrin.h>
#define CountLeadingZeros32(x) (int)__lzcnt(x)
#else
#error "unsupported compiler"
#endif

static const unsigned UTF8InvalidLength = 0;

static inline unsigned UTF8GetCharLength(char ch) {
	if (ch == 0)
		return UTF8InvalidLength;

	int lz = CountLeadingZeros32(~((unsigned)ch << 24));

	if (lz > 4 || lz == 1)
		return UTF8InvalidLength;

	if (lz == 0)
		return 1;

	return (unsigned)lz;
}

static INIStringSlice bound(const char *base, INIStringSlice s) {
	const char *end = s.base + s.size;
	
	if (end < base)
		end = base;

	return (INIStringSlice){base, end - base};
}

static const char *find_if(INIStringSlice s, INILocation *l, bool (*pred)(char)) {
	const char *end = s.base + s.size;
	while (true) {
		if (s.base >= end)
			return NULL;
	
		if (pred(*s.base))
			break;

		if (*s.base == '\n') {
			l->line++;
			l->column = 1; 
		} else
			l->column++;
			
		unsigned len = UTF8GetCharLength(*s.base);
		if (len == UTF8InvalidLength)
			return NULL;

		s.base += len;
	}
	
	return s.base;
}

static bool is_ws(char c) {
	return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static bool is_not_ws(char c) {
	return !is_ws(c);
}

static bool is_lf(char c) {
	return c == '\n';
}

static bool is_ws_or_closing_bracket(char c) {
	return is_ws(c) || c == ']';
}

static bool is_closing_bracket(char c) {
	return c == ']';
}

static bool is_lf_or_not_ws(char c) {
	return is_lf(c) || is_not_ws(c);
}

static bool is_ws_or_eq(char c) {
	return is_ws(c) || c == '=';
}

static bool is_quote(char c) {
	return c == '"';
}

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
) {
	INIStringSlice section = {NULL, 0};

	const char *input_p = input.base;
	const char *input_end = input.base + input.size;

	INILocation l = {1, 1};

	bool ok = true;

	while (input_p && input_p < input_end) {
		// Skip whitespace chars.

		if (!(input_p = find_if(bound(input_p, input), &l, is_not_ws)))
			break;

		if (*input_p == '[') {
			// An opening bracket designates a section name.

			INIStringSlice nsection;

			// The first non-whitespace char is the beginning of the section name.
			nsection.base = find_if(bound(input_p + 1, input), &l, is_not_ws);
			l.column++;

			if (nsection.base && *nsection.base != ']') {
				// Now need to find the optional whitespace.
				const char *nsection_end = find_if(
					bound(nsection.base, input),
					&l, is_ws_or_closing_bracket
				);

				if (nsection_end) {
					nsection.size = nsection_end - nsection.base;

					// Find the closing bracket.
					const char *clbr = find_if(
						bound(nsection_end, input),
						&l, is_closing_bracket
					);

					if (clbr) {
						// Successfully parsed a section name!
						section = nsection;

						// Report and skip garbage if any.

						const char *lf = find_if(
							bound(clbr + 1, input),
							&l, is_lf_or_not_ws
						);

						l.column++;

						if (lf) {
							if (*lf == '\n') {
								l.line++;
								l.column = 1;
								input_p = lf + 1;
							} else {
								ok = false;

								handleError(
									errorHandlerCtx, l,
									"unexpected junk"									
								);
							
								input_p = find_if(
									bound(lf + 1, input),
									&l, is_lf
								);
							}
						} else {
							input_p = NULL;
						}
					} else {
						ok = false;

						handleError(
							errorHandlerCtx, l,
							"expected a closing bracket"
						);

						input_p = NULL;
					}
				} else {
					ok = false;

					handleError(
						errorHandlerCtx, l,
						"expected a closing bracket"
					);
				
					input_p = NULL;
				}
			} else {
				ok = false;
			
				handleError(
					errorHandlerCtx, l,
					"expected a section name"
				);

				if (nsection.base) {
					// Got an empty section name. Proceed to the next line.
					input_p = find_if(
						bound(nsection.base + 1, input),
						&l, is_lf
					);
				} else {
					// Got nothing after the opening bracket.
					input_p = NULL;
				}
			}
		} else if (*input_p == '#') {
			input_p = find_if(bound(input_p, input), &l, is_lf);
		} else {
			INIStringSlice key = {input_p, 0};

			const char *key_end = find_if(
				bound(input_p, input),
				&l, is_ws_or_eq
			);

			if (key_end) {
				key.size = key_end - key.base;

				const char *eq = find_if(
					bound(key_end, input),
					&l, is_not_ws
				);

				if (eq && *eq == '=') {
					l.column++;

					const char *quote_or_value = find_if(
						bound(eq + 1, input),
						&l, is_lf_or_not_ws
					);

					if (quote_or_value) {
						if (*quote_or_value == '"') {
							INIStringSlice value = {quote_or_value + 1, 0};

							l.column++;

							const char *value_end = find_if(
								bound(value.base, input),
								&l, is_quote
							);

							if (value_end) {
								value.size = value_end - value.base;
	
								handleEntry(
									entryHandlerCtx,
									section,
									key,
									value
								);

								l.column++;
								input_p = value_end + 1;
							} else {
								ok = false;
							
								handleError(
									errorHandlerCtx, l,
									"expected a quote"
								);

								input_p = NULL;
							}
						} else {
							INIStringSlice value = {quote_or_value, 0};

							const char *value_end = find_if(
								bound(quote_or_value, input),
								&l, is_lf
							);

							if (!value_end)
								value_end = input_end;

							value.size = value_end - value.base;

							handleEntry(
								entryHandlerCtx,
								section,
								key,
								value
							);

							input_p = value_end;
						}
					} else {
						handleEntry(
							entryHandlerCtx,
							section,
							key,
							(INIStringSlice){NULL, 0}
						);
					
						input_p = NULL;
					}
				} else {
					ok = false;
					
					handleError(
						errorHandlerCtx, l,
						"expected '='"
					);

					if (eq) {
						input_p = find_if(
							bound(eq, input),
							&l, is_lf
						);
					} else
						input_p = NULL;
				}
			} else {
				ok = false;

				handleError(
					errorHandlerCtx, l,
					"expected '='"
				);

				input_p = NULL;
			}
		}
	}

	return ok;
}
