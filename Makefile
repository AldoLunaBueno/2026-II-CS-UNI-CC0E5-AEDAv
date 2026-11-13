CXX        := g++
CXXFLAGS   := -std=c++23
TARGET     := main
SRCS       := main.cpp demos.cpp
PR_OUT     := tmp/pr_comment.md
PR_SCRIPT  := scripts/gen_pr_comment.sh

.PHONY: all run pr-comment clean

all: clean $(TARGET) run pr-comment

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) $(SRCS) -o $(TARGET)

run: $(TARGET)
	./$(TARGET)

pr-comment:
	@bash $(PR_SCRIPT) README.md $(PR_OUT)

clean:
	rm -rf $(TARGET) tmp
	@clear 2>/dev/null || printf "\033c"
