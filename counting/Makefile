#! /usr/bin/make

# Add binaries here (each name relative to top of tree!).
BINARIES+=counting/hare_clarke counting/vacancy counting/test_fraction

# Add any extra tests to run here (each name relative to top of tree!).
EXTRATESTS+=counting/hare_clarke_test.sh counting/vacancy_test.sh

# Include *_test.c automatically.
CTESTS+=$(foreach tc, $(wildcard counting/*_test.c), $(tc:.c=))

# This needs to come before any rules, so binaries is the default.
ifndef MASTER
  binaries tests clean dep TAGS:
	$(MAKE) -C .. $@ DIR="`pwd`"
endif # MASTER

counting/hare_clarke: counting/count.o counting/ballot_iterators.o counting/candidate_iterators.o counting/fraction.o  common/evacs.o counting/report.o counting/fetch.o common/database.o
counting/hare_clarke_ARGS:=-lpq 

counting/test_fraction: counting/count.o counting/ballot_iterators.o counting/candidate_iterators.o counting/fraction.o  common/evacs.o counting/report.o counting/fetch.o common/database.o
counting/test_fraction_ARGS:=-lpq

counting/vacancy: counting/count.o counting/ballot_iterators.o counting/candidate_iterators.o counting/fraction.o  common/evacs.o counting/report.o counting/fetch.o common/database.o
counting/vacancy_ARGS:=-lpq 

counting/hare_clarke_test: counting/count.o counting/ballot_iterators.o counting/candidate_iterators.o counting/fraction.o common/safe.o common/evacs.o counting/report.o

counting/vacancy_test: counting/count.o counting/ballot_iterators.o counting/candidate_iterators.o counting/fraction.o  common/evacs.o counting/report.o

counting/hare_clarke_VC3_test: counting/count.o counting/ballot_iterators.o counting/candidate_iterators.o counting/fraction.o common/safe.o common/evacs.o counting/report.o

counting/hare_clarke_VC4_test: counting/count.o counting/ballot_iterators.o counting/candidate_iterators.o counting/fraction.o common/safe.o common/evacs.o counting/report.o

counting/hare_clarke_VC5_test: counting/count.o counting/ballot_iterators.o counting/candidate_iterators.o counting/fraction.o common/safe.o common/evacs.o counting/report.o

counting/hare_clarke_VC6_test: counting/count.o counting/ballot_iterators.o counting/candidate_iterators.o counting/fraction.o common/safe.o common/evacs.o counting/report.o

counting/hare_clarke_test: counting/count.o

counting/report_test: common/evacs.o common/safe.o

counting/hare_clarke_test.sh-run: counting/hare_clarke_test






