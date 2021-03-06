#pragma once

#include <ch_stl/filesystem.h>
#include <ch_stl/gap_buffer.h>
#include <ch_stl/hash.h>
#include "draw.h"
#include "parsing.h"

using Buffer_ID = usize;
const usize invalid_buffer_id = 0;

CH_FORCEINLINE u64 hash(Buffer_ID id) {
	return ch::fnv1_hash(&id, sizeof(Buffer_ID));
}

/**
 * Gets the column size of char
 *
 * @TODO: Move to somewhere else
 */
u32 get_char_column_size(u32 c);

enum Line_Ending {
	LE_NIX, // \n
	LE_CRLF // \r\n
};

CH_FORCEINLINE const char* get_line_ending_display(Line_Ending ending) {
	switch (ending) {
		case LE_NIX:
			return "unix";
		case LE_CRLF:
			return "crlf";
	}
	return nullptr;
}

/** Encoding of the loaded buffer. Default for buffers will be user defined. As more encodings are added this needs to be updated. */
enum Buffer_Encoding {
	BE_ANSI,
	BE_UTF8
};

CH_FORCEINLINE const char* get_buffer_encoding_display(Buffer_Encoding encoding) {
	switch (encoding) {
		case BE_ANSI: 
			return "ansi";
		case BE_UTF8:
			return "utf-8";
	}
	return nullptr;
}

enum Buffer_Flags {
	BF_File = 1,
	BF_Scratch = 1 << 1,
	BF_ReadOnly = 1 << 2,
};

/**
 * Wrapper around gap buffer that keeps cached data about the contents of the gap buffer
 *
 * @see ch:Gap_Buffer
 */
struct Buffer {
	Buffer_ID id = invalid_buffer_id;

	/** Gap Buffer used to reduce moving bytes with every char press. */
	ch::Gap_Buffer<u8> gap_buffer;

	/** Absolute path to the file this buffer will save to. */
	ch::Path absolute_path;

	/** Name used for powerline display and buffer lookup. */
	ch::String name;

	/**
	 * Linear table where index is line index and value is the size of the line with eol characters in bytes
	 * Used for moving up and down lines in a fast manner
	 *
	 * @see refresh_eol_table
	 */
	ch::Array<u32> eol_table;

	/**
	 * Linear table where index is line index and value is the size of line with eol characters in col count
	 * Used for rendering
	 *
	 * @see refresh_line_column_table
	 */
	ch::Array<u32> line_column_table;

	/**
	 * Current line endings used in this buffer. 
	 *
	 * @see load_file_into_buffer for how we determine what line_ending to use
	 */
	Line_Ending line_ending = LE_NIX;

	/**
	 * Encoding of the current buffer. Default is defined by user but is also dtermined if file is loaded
	 *
	 * @see load_file_into_buffer
	 */
	Buffer_Encoding encoding = BE_UTF8;

	/**
	 * Attributes to give details about what type of buffer this is.
	 *
	 * @see Buffer_Flags
	 */
	u8 flags = 0;

	/**
	 * Keeps track if file is dirty
	 * 
	 * @temp until we have undo/redo
	 */
	bool is_dirty = false;

	bool disable_parse = false;
    bool syntax_dirty = true;
    ch::Array<parsing::Lexeme> lexemes;
    f64 lex_time = 0;
    f64 parse_time = 0;
    u64 lex_parse_count = 0;

	Buffer() = default;
	Buffer(Buffer_ID _id);

	/**
	 * Loads a file into this buffer. Will also determine what line endings to use and what encoding the file is.
	 *
	 * @param path is the path to the file. Can be relative or absolute.
	 * @returns true if the file was loaded
	 */
	bool load_file_into_buffer(const ch::Path& path);

	/**
	 * Saves to a file at the listed path.
	 *
	 * @returns true if save was successful
	 */
	bool save_file_to_path();

	/** Empties the gap buffer and resets all cached state. */
    void empty();

	/** Frees all dynamic memory. */
	void free();

	void add_char(u32 c, usize index);
	void remove_char(usize index);

	void print_to(const char* fmt, ...);

	/** 
	 * Clears cached data and runs through entire buffer to rebuild it. 
	 * 
	 * @speed this is O(n) but requires decoding
	 * @note this will probably be deprecated. Just using now due to laziness
	 */
	void refresh_line_tables();

	/**
	 * Finds the next codepoint based on file encoding
	 * Returns gap_buffer.count() for end of buffer
	 *
	 * @param index is the location to start searching at
	 * @return is the found "next" index
	 */
	usize find_next_char(usize index);

	/**
	 * Finds prev codepoint based on file encoding
	 * Returns 0 for beginning of buffer
	 *
	 * @param index is the location to start searching at
	 * @return is the found "prev" index
	 */
	usize find_prev_char(usize index);

	/**
	 * Decodes character at codepoint index
	 *
	 * @param index is the char codepoint
	 * @returns decoded char based on file encoding
	 */
	u32 get_char(usize index);

	u64 get_index_from_line(u64 line) const;
	u64 get_line_from_index(u64 index) const;
    u64 get_wrapped_line_from_index(u64 index, u64 max_line_width) const;

	/** @temp */
	void mark_file_dirty();
};

/** Creates a new buffer and @returns the new buffer's id. */
Buffer_ID create_buffer();

/** 
 * Finds the buffer via Buffer_ID. 
 *
 * @param id is the buffer's id we're trying to find. 
 * @returns a pointer to the actual buffer. 
 */
Buffer* find_buffer(Buffer_ID id);

/** 
 * Removes the buffer with the id given. 
 *
 * @param id is the buffer's id we're trying to remove.
 * @returns true if the buffer was removed. 
 */
bool remove_buffer(Buffer_ID id);