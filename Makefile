NAME=bcpp

DEPDIR=.dep
OBJDIR=.obj
SRCDIR=src

SRC = main.c
CPPSRC =
MSRC =
ALL_SRC= $(MSRC) $(CPPSRC) $(SRC)


OBJ1=$(ALL_SRC:.c=.o)
OBJ2=$(OBJ1:.m=.o)
OBJ3=$(OBJ2:.cpp=.cpp.o)
OBJ=$(addprefix $(OBJDIR)/,$(OBJ3))

WARNINGS_ = -Wno-macro-redefined -Wno-sizeof-array-argument -Wno-unknown-warning-option -Wno-sizeof-pointer-div -Wno-unneeded-internal-declaration -Wno-unused-parameter -Wno-unused-variable -Wno-unused-private-field -Wno-comment -Wno-trigraphs
INCLUDES_ = 

CFLAGS += -Wall -Wextra -Werror $(WARNINGS_) $(INCLUDES_) -g -trigraphs -O2
CPPFLAGS += -Wall -Wextra -Werror $(WARNINGS_) $(INCLUDES_) -std=c++11 -g
MFLAGS += -g

CC = gcc
MC = clang
CPPC = g++
LINKER = g++

all: $(OBJDIR) $(DEPDIR) $(NAME)

run: $(OBJDIR) $(DEPDIR) $(NAME)
	./$(NAME)

$(OBJDIR):
	mkdir $(OBJDIR)

$(DEPDIR):
	mkdir $(DEPDIR)

$(NAME): $(OBJ)
	$(LINKER) $(OBJ) -o $(NAME)

clean:
	rm -rf $(OBJDIR)
	rm -rf $(DEPDIR)

fclean: clean
	rm -rf $(NAME)

re: fclean all




# .obj rules and header dependencies

DEPFLAGS=-MT $@ -MMD -MP

$(OBJDIR)/%.cpp.o : %.cpp
$(OBJDIR)/%.cpp.o : %.cpp $(DEPDIR)/%.cpp.cd | .dep
	mkdir -p $(dir $@)
	mkdir -p $(dir $(DEPDIR)/$*.cpp.cd)
	$(CPPC) -c $(CPPFLAGS) $(DEPFLAGS) -MF $(DEPDIR)/$*.cpp.cd.compiled $< -o $@
	mv -f $(DEPDIR)/$*.cpp.cd.compiled $(DEPDIR)/$*.cpp.cd
$(OBJDIR)/%.o : %.c
$(OBJDIR)/%.o : %.c $(DEPDIR)/%.cd | .dep
	mkdir -p $(dir $@)
	mkdir -p $(dir $(DEPDIR)/$*.cd)
	$(CC) -c $(CFLAGS) $(DEPFLAGS) -MF $(DEPDIR)/$*.cd.compiled $< -o $@
	mv -f $(DEPDIR)/$*.cd.compiled $(DEPDIR)/$*.cd
$(OBJDIR)/%.o : %.m
$(OBJDIR)/%.o : %.m $(DEPDIR)/%.md | .dep
	mkdir -p $(dir $@)
	mkdir -p $(dir $(DEPDIR)/$*.cd)
	$(MC) -c $(MFLAGS) $(DEPFLAGS) -MF $(DEPDIR)/$*.md.compiled $< -o $@
	mv -f $(DEPDIR)/$*.md.compiled $(DEPDIR)/$*.md

$(DEPDIR)/%.md: ;
$(DEPDIR)/%.cd: ;
.PRECIOUS: $(DEPDIR)/%.md $(DEPDIR)/%.cd

include $(shell find .dep -type f -name '*.md') $(shell find .dep -type f -name '*.cd')







