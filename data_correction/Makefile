#! /usr/bin/make

# Add binaries here (each name relative to top of tree!).
BINARIES+=data_correction/batch_edit_bin


# Add any extra tests to run here (each name relative to top of tree!).
EXTRATESTS+=

# Include *_test.c automatically.
CTESTS+=$(foreach tc, $(wildcard data_correction/*_test.c), $(tc:.c=))

# This needs to come before any rules, so binaries is the default.
ifndef MASTER
  binaries tests clean dep TAGS:
	$(MAKE) -C .. $@ DIR="`pwd`"
endif # MASTER


data_correction/batch_edit_bin: data_correction/batch_edit.o common/evacs.o common/database.o  common/safe.o common/find_errors.o common/batch.o common/createtables.o voting_client/vote_in_progress.o  data_entry/accumulate_deo_preferences.o  data_entry/interpret_deo_keystroke.o data_entry/confirm_paper.o data_entry/get_paper_version.o data_entry/handle_end_batch_screen.o data_entry/update_deo_preference.o  data_entry/delete_deo_preference.o data_entry/update_deo_preference.o data_entry/delete_deo_preference.o data_correction/batch_edit.o  data_entry/voter_electorate.o common/current_paper_index.o common/ballot_contents.o common/get_electorate_ballot_contents.o common/safe.o common/database.o common/batch.o common/evacs.o common/find_errors.o common/language.o  common/http.o common/socket.o common/cursor.o  voting_client/image.o voting_client/message.o voting_client/main_screen.o voting_client/get_rotation.o  data_entry/move_deo_cursor.o voting_client/vote_in_progress.o common/cursor.o   voting_client/draw_group_entry.o voting_client/get_img_at_cursor.o voting_server/fetch_rotation.o voting_client/get_rotation.o data_entry/prompts.o data_entry/enter_paper.o data_entry/dummy_audio.o

data_correction/batch_edit_bin_ARGS:=-lpq -lX11  -L/usr/X11R6/lib -lpng

data_correction/batch_edit_test: common/evacs.o common/database.o  common/safe.o common/find_errors.o common/batch.o common/createtables.o voting_client/vote_in_progress.o common/ballot_contents.o data_entry/get_paper_version.o
data_correction/batch_edit_test_ARGS:=-lpq
data_correction/batch_edit_print_test: common/evacs.o common/database.o  common/safe.o common/find_errors.o common/batch.o common/createtables.o voting_client/vote_in_progress.o common/ballot_contents.o data_entry/get_paper_version.o
data_correction/batch_edit_print_test_ARGS:=-lpq
