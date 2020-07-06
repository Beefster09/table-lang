
#define TWO_BYTES      0x0080
#define THREE_BYTES    0x0800
#define FOUR_BYTES    0x10000

#define CONT_MASK 0x3f
#define CONT 0x80

char* utf8_write(char* buffer, int codepoint) {
	if (codepoint < TWO_BYTES) {
		*buffer++ = codepoint;
	}
	else if (codepoint < THREE_BYTES) {
		*buffer++ = ((codepoint >> 6)  & 0x1f)      | 0xc0;
		*buffer++ = (codepoint         & CONT_MASK) | CONT;
	}
	else if (codepoint < FOUR_BYTES) {
		*buffer++ = ((codepoint >> 12) & 0x0f)      | 0xe0;
		*buffer++ = ((codepoint >> 6)  & CONT_MASK) | CONT;
		*buffer++ = (codepoint         & CONT_MASK) | CONT;
	}
	else {
		*buffer++ = ((codepoint >> 18) & 0x07)      | 0xf0;
		*buffer++ = ((codepoint >> 12) & CONT_MASK) | CONT;
		*buffer++ = ((codepoint >> 6)  & CONT_MASK) | CONT;
		*buffer++ = (codepoint         & CONT_MASK) | CONT;
	}
	return buffer;
}
