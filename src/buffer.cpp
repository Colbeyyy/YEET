#include "buffer.h"
#include "parsing.h"
#include "string.h"
#include "os.h"
#include "memory.h"
#include "draw.h"
#include "font.h"
#include "keys.h"
#include "editor.h"
#include "input.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

u32& Buffer::operator[](size_t index) {
	assert(index < get_count(*this));

	if (data + index < gap) {
		return data[index];
	}
	else {
		return data[index + gap_size];
	}
}

u32 Buffer::operator[](size_t index) const {
	assert(index < get_count(*this));

	if (data + index < gap) {
		return data[index];
	}
	else {
		return data[index + gap_size];
	}
}

Buffer make_buffer(Buffer_ID id) {
    Buffer result = {0};
	result.id = id;
	return result;
}

bool buffer_load_from_file(Buffer* buffer, const char* path) {
	FILE* file = fopen(path, "rb");
	if (!file) {
		return false;
	}
	
	buffer->path = path;
	fseek(file, 0, SEEK_END);
	const size_t file_size = ftell(file);
	fseek(file, 0, SEEK_SET);

	buffer->allocated = file_size + DEFAULT_GAP_SIZE;
	buffer->data = (u32*)c_alloc(buffer->allocated * sizeof(u32));

	size_t extra = 0;
	u32* current_char = buffer->data;
	size_t line_size = 0;
	while (current_char != buffer->data + file_size) {
		const u8 c = fgetc(file);
		if (c == '\r') {
			extra += 1;
			continue;
		} 
		*current_char = (u32)c;
		current_char += 1;
		line_size += 1;
		if (c == '\n'){
			array_add(&buffer->eol_table, line_size);
			line_size = 0;
		}
	}
    // @Cleanup: why does + 1 make things work here?
   // array_add(&buffer->eol_table, line_size - extra);
	fclose(file);

	buffer->gap = buffer->data + file_size - extra;
	buffer->gap_size = DEFAULT_GAP_SIZE + extra;

	// @NOTE(Colby): Basic file name parsing
	for (size_t i = buffer->path.count - 1; i >= 0; i--) {
		if (buffer->path[i] == '.') {
			buffer->language.data = buffer->path.data + i + 1;
			buffer->language.count = buffer->path.count - i - 1;
			buffer->language.allocated = buffer->language.count;
		}

		if (buffer->path[i] == '/' || buffer->path[i] == '\\') {
			buffer->title.data = buffer->path.data + i + 1;
			buffer->title.count = buffer->path.count - i - 1;
			break;
		}
	}

    // @Temporary
    parse_syntax(buffer);
	return true;
}

void buffer_init_from_size(Buffer* buffer, size_t size) {
	if (size < DEFAULT_GAP_SIZE) size = DEFAULT_GAP_SIZE;

	buffer->allocated = size;
	buffer->data = (u32*)c_alloc(size * sizeof(u32));

	buffer->gap = 0;
	buffer->gap_size = size;
	array_add(&buffer->eol_table, (size_t)0);

	buffer->language = "cpp";
}

void buffer_resize(Buffer* buffer, size_t new_gap_size) {
	// @NOTE(Colby): Check if we have been initialized
	assert(buffer->data);
	assert(buffer->gap_size == 0);

	const size_t old_size = buffer->allocated;
	const size_t new_size = old_size + new_gap_size;

	u32 *old_data = buffer->data;
	u32 *new_data = (u32 *)c_realloc(old_data, new_size * sizeof(u32));
	if (!new_data) {
		assert(false);
	}

	buffer->data = new_data;
	buffer->allocated = new_size;
	buffer->gap = (buffer->data + old_size);
	buffer->gap_size = new_gap_size;
}

static void move_gap_to_index(Buffer* buffer, size_t index) {
	const size_t buffer_count = get_count(*buffer);
	assert(index < buffer_count);

	u32* index_ptr = &(*buffer)[index];

	if (index_ptr < buffer->gap) {
		const size_t amount_to_move = buffer->gap - index_ptr;

		memcpy(buffer->gap + buffer->gap_size - amount_to_move, index_ptr, amount_to_move * sizeof(u32));

		buffer->gap = index_ptr;
	} else {
		const size_t amount_to_move = index_ptr - (buffer->gap + buffer->gap_size);

		memcpy(buffer->gap, buffer->gap + buffer->gap_size, amount_to_move * sizeof(u32));

		buffer->gap += amount_to_move;
	}
}

u32* get_index_as_cursor(Buffer* buffer, size_t index) {
	if (buffer->data + index <= buffer->gap) {
		return buffer->data + index;
	} 

	return buffer->data + buffer->gap_size + index;
}

void add_char(Buffer* buffer, u32 c, size_t index) {
	if (buffer->gap_size <= 0) {
		buffer_resize(buffer, DEFAULT_GAP_SIZE);
	}

	move_gap_to_index(buffer, index);

	u32* cursor = get_index_as_cursor(buffer, index);
	*cursor = c;
	buffer->gap += 1;
	buffer->gap_size -= 1;

    size_t index_line = 0;
	size_t index_column = 0;

	size_t current_index = 0;
	for (size_t i = 0; i < buffer->eol_table.count; i++) {
		size_t line_size = buffer->eol_table[i];

		if (index < current_index + line_size) {
			index_column = index - current_index;
			break;
		}

		current_index += line_size;
		index_line += 1;
	}
	if (is_eol(c)) {
        const size_t line_size = buffer->eol_table[index_line];
		buffer->eol_table[index_line] = index_column + 1;
		array_add_at_index(&buffer->eol_table, line_size - index_column, index_line + 1);
	} else {
        buffer->eol_table[index_line] += 1;
    }

	parse_syntax(buffer);
}

void remove_at_index(Buffer* buffer, size_t index) {
	const size_t buffer_count = get_count(*buffer);
	assert(index < buffer_count);

	u32* cursor = get_index_as_cursor(buffer, index);
	if (cursor == buffer->data) {
		return;
	}

	move_gap_to_index(buffer, index);

	size_t index_line = 0;
	size_t current_index = 0;
	for (size_t i = 0; i < buffer->eol_table.count; i++) {
		size_t line_size = buffer->eol_table[i];

		if (index < current_index + line_size) {
			break;
		}

		current_index += line_size;
		index_line += 1;
	}

	buffer->eol_table[index_line] -= 1;
	
    const u32 c = (*buffer)[index - 1];
	if (is_eol(c)) {
		const size_t amount_on_line = buffer->eol_table[index_line];
		array_remove_index(&buffer->eol_table, index_line);
		buffer->eol_table[index_line - 1] += amount_on_line;
    }

	buffer->gap -= 1;
	buffer->gap_size += 1;

	parse_syntax(buffer);
}

void remove_between(Buffer* buffer, size_t first, size_t last) {
    // @SLOW(Colby): This is slow as shit
    for (size_t i = first; i < last; i++) {
        remove_at_index(buffer, i);
    }
}

size_t get_count(const Buffer& buffer) {
	return buffer.allocated - buffer.gap_size;
}

size_t get_line_index(const Buffer& buffer, size_t index) {
	assert(index < buffer.eol_table.count);
	
	size_t result = 0;
	for (size_t i = 0; i < index; i++) {
		result += buffer.eol_table[i];
	}

	return result;
}


Buffer* get_buffer(Buffer_View* view) {
	Buffer* result = editor_find_buffer(view->editor, view->id);
	assert(result);
	return result;
}

void refresh_cursor_info(Buffer_View* view, bool update_desired /*=true*/) {
	Buffer* buffer = get_buffer(view);
	view->current_line_number = 0;
	view->current_column_number = 0;
	for (size_t i = 0; i < view->cursor; i++) {
		const u32 c = (*buffer)[i];

		view->current_column_number += 1;

		if (is_eol(c)) {
			view->current_line_number += 1;
			view->current_column_number = 0;
		}
	}

	if (update_desired) {
		view->desired_column_number = view->current_column_number;
	}

	const float font_height = FONT_SIZE;
	const size_t lines_scrolled = (size_t)(view->current_scroll_y / font_height);
	const size_t lines_in_window = (size_t)(os_window_height() / font_height) - 2;

	if (view->current_line_number < lines_scrolled) {
		view->target_scroll_y = view->current_line_number * font_height;
		view->current_scroll_y = view->target_scroll_y;
	}

	if (view->current_line_number > lines_scrolled + lines_in_window) {
		view->target_scroll_y += (view->current_line_number - (lines_scrolled + lines_in_window)) * font_height;
		view->current_scroll_y = view->target_scroll_y;
	}
}

void move_cursor_vertical(Buffer_View* view, s64 delta) {
	Buffer* buffer = get_buffer(view);

	assert(delta != 0);

	const size_t buffer_count = get_count(*buffer);
	if (buffer_count == 0) {
		return;
	}

	size_t new_line = view->current_line_number + delta;
	if (delta < 0 && view->current_line_number <= (u64)-delta) {
		new_line = 0;
	}
	else if (new_line >= buffer->eol_table.count) {
		new_line = buffer->eol_table.count - 1;
	}

	const size_t line_index = get_line_index(*buffer, new_line);

	size_t new_cursor_index = line_index;
	if (buffer->eol_table[new_line] <= view->desired_column_number) {
		new_cursor_index += buffer->eol_table[new_line] - 1;
	}
	else {
		new_cursor_index += view->desired_column_number;
	}

	view->cursor = new_cursor_index;
	refresh_cursor_info(view, false);
}

void seek_line_start(Buffer_View* view) {
	// @Todo: indent-sensitive home seeking
	view->cursor -= view->current_column_number;
	refresh_cursor_info(view);
}

void seek_line_end(Buffer_View* view) {
	Buffer* buffer = get_buffer(view);
	if (buffer->eol_table.count) {
		const size_t line_length = buffer->eol_table[view->current_line_number];
		if (!line_length) {
			assert(view->current_column_number == 0);
		}
		else {
			view->cursor += line_length - view->current_column_number - 1;
		}
	}
	refresh_cursor_info(view);
}

void seek_horizontal(Buffer_View* view, bool right) {
	Buffer* buffer = get_buffer(view);
	const size_t buffer_count = get_count(*buffer);

	if (right) {
		if (view->cursor == buffer_count) {
			return;
		}
		view->cursor += 1;
	}
	else {
		if (view->cursor == 0) {
			return;
		}
		view->cursor -= 1;
	}

	while (view->cursor > 0 && view->cursor < buffer_count) {
		const u32 c = (*buffer)[view->cursor];

		if (is_whitespace(c) || is_symbol(c)) break;

		if (right) {
			view->cursor += 1;
		}
		else {
			view->cursor -= 1;
		}
	}

	refresh_cursor_info(view);
}

static void char_entered(void* owner, Event* event) {
	Buffer_View* view = (Buffer_View*)owner;
	Buffer* buffer = get_buffer(view);

	add_char(buffer, event->c, view->cursor);
	view->cursor += 1;
    view->selection += 1;
    refresh_cursor_info(view);
}

static void key_pressed(void* owner, Event* event) {
	Buffer_View* view = (Buffer_View*)owner;
	Editor_State* editor = view->editor;
	Input_State* input = &editor->input_state;
	Buffer* buffer = get_buffer(view);
	const size_t buffer_count = get_count(*buffer);

	switch (event->c) {
	case KEY_LEFT:
		if (view->cursor > 0) {
			if (input->ctrl_is_down) {
				seek_horizontal(view, false);
				break;
			}
			view->cursor -= 1;
            if (!input->shift_is_down) {
                view->selection = view->cursor;
            }
			refresh_cursor_info(view);
		}
		break;
	case KEY_RIGHT:
		if (view->cursor < buffer_count - 1) {
			if (input->ctrl_is_down) {
				seek_horizontal(view, true);
				break;
			}
			view->cursor += 1;
            if (!input->shift_is_down) {
                view->selection = view->cursor;
            }
			refresh_cursor_info(view);
		}
		break;
	case KEY_ENTER:
		add_char(buffer, '\n', view->cursor);
		view->cursor += 1;
        view->selection = view->cursor;
		refresh_cursor_info(view);
		break;
	case KEY_UP:
		move_cursor_vertical(view, -1);
        if (!input->shift_is_down) {
            view->selection = view->cursor;
        }
		break;
	case KEY_DOWN:
		move_cursor_vertical(view, 1);
        if (!input->shift_is_down) {
            view->selection = view->cursor;
        }
		break;
	case KEY_HOME:
		seek_line_start(view);
        if (!input->shift_is_down) {
            view->selection = view->cursor;
        }
		break;
	case KEY_END:
		seek_line_end(view);
        if (!input->shift_is_down) {
            view->selection = view->cursor;
        }
		break;
	case KEY_BACKSPACE:
		if (view->cursor > 0) {
            remove_at_index(buffer, view->cursor);
			view->cursor -= 1;
            view->selection = view->cursor;
			refresh_cursor_info(view);
		}
		break;
	case KEY_DELETE:
		const size_t buffer_count = get_count(*buffer);
		if (view->cursor < buffer_count - 1) {
			remove_at_index(buffer, view->cursor + 1);
			refresh_cursor_info(view);
		}
		break;
	}
}

void buffer_view_lost_focus(Buffer_View* view) {
	Editor_State* editor = view->editor;

	unbind_event_listener(&editor->input_state, make_event_listener(view, char_entered, ET_Char_Entered));
	unbind_event_listener(&editor->input_state, make_event_listener(view, key_pressed, ET_Key_Pressed));
}

void buffer_view_gained_focus(Buffer_View* view) {
	Editor_State* editor = view->editor;

	bind_event_listener(&editor->input_state, make_event_listener(view, char_entered, ET_Char_Entered));
	bind_event_listener(&editor->input_state, make_event_listener(view, key_pressed, ET_Key_Pressed));
}
