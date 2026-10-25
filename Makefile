CXX      = g++
CXXFLAGS = -std=c++17 -I include -Wall

OBJ_DIR  = object
SRC_DIR  = source

SRCS     = $(SRC_DIR)/main.cpp \
           $(SRC_DIR)/analizadorfs.cpp \
           $(SRC_DIR)/driver_base.cpp \
           $(SRC_DIR)/driver_fat.cpp \
           $(SRC_DIR)/driver_ext.cpp \
           $(SRC_DIR)/driver_ntfs.cpp

OBJS     = $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(SRCS))
TARGET   = tpfs

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@ -lm

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ_DIR)/*.o $(TARGET)
