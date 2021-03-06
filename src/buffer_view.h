#pragma once

#include "buffer.h"

const f32 min_width_ratio = 0.2f;

struct Buffer_View {
	Buffer_ID the_buffer = 0;
	f32 width_ratio = 0.5f;

	usize cursor = 0;
	usize selection = 0;

	u64 desired_column = 0;
	u64 current_column = 0;
	u64 current_line = 0;

	f32 current_scroll_y = 0.f;
	f32 target_scroll_y = 0.f;

	bool show_cursor = true;
	f32 cursor_blink_time = 0.f;

	CH_FORCEINLINE bool has_selection() const { return cursor != selection; }

	CH_FORCEINLINE void reset_cursor_timer() {
		show_cursor = true;
		cursor_blink_time = 0.f;
	}

	void remove_selection();
	
	/**
	 * Updates column and line data
	 *
	 * @param update_desired_col if true sets desired_column to current_column
	 */
	void update_column_info(bool update_desired_col = false);
	void ensure_cursor_in_view();

	void on_char_entered(u32 c);
};

void tick_views(f32 dt);

Buffer_View* get_focused_view();

usize push_view(Buffer_ID the_buffer);
usize insert_view(Buffer_ID the_buffer, usize index);
bool remove_view(usize view_index);
Buffer_View* get_view(usize index);

