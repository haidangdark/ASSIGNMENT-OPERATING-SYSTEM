INC = -Iinclude
LIB = -lpthread

SRC = src
OBJ = obj
INCLUDE = include

CC = gcc
DEBUG = -g
CFLAGS = -Wall -c $(DEBUG)
LFLAGS = -Wall $(DEBUG)

vpath %.c $(SRC)
vpath %.h $(INCLUDE)

MAKE = $(CC) $(INC) 

# Object files needed by modules
MEM_OBJ = $(addprefix $(OBJ)/, paging.o mem.o cpu.o loader.o)
SYSCALL_OBJ = $(addprefix $(OBJ)/, syscall.o  sys_mem.o sys_listsyscall.o)
# === ĐÃ THÊM === thêm handler syscall mới
SYSCALL_OBJ += $(addprefix $(OBJ)/, sys_xxxhandler.o)

OS_OBJ = $(addprefix $(OBJ)/, cpu.o mem.o loader.o queue.o os.o sched.o timer.o mm-vm.o mm64.o mm.o mm-memphy.o libstd.o libmem.o)
OS_OBJ += $(SYSCALL_OBJ)
SCHED_OBJ = $(addprefix $(OBJ)/, cpu.o loader.o)
HEADER = $(wildcard $(INCLUDE)/*.h)
 
all: os
#mem sched os

# Just compile memory management modules
mem: $(MEM_OBJ)
	$(MAKE) $(LFLAGS) $(MEM_OBJ) -o mem $(LIB)

# Just compile scheduler
sched: $(SCHED_OBJ)
	$(MAKE) $(LFLAGS) $(MEM_OBJ) -o sched $(LIB)

# Compile syscall
# (sinh src/syscalltbl.lst để syscall.c include được "syscalltbl.lst")
syscalltbl.lst: $(SRC)/syscall.tbl
	@echo $(OS_OBJ)
	chmod +x $(SRC)/syscalltbl.sh
	$(SRC)/syscalltbl.sh $< $(SRC)/$@ 
#	mv $(OBJ)/syscalltbl.lst $(INCLUDE)/

# === ĐÃ THÊM === đảm bảo syscall.o re-build khi bảng syscall thay đổi
$(OBJ)/syscall.o: $(SRC)/syscalltbl.lst

# Compile the whole OS simulation
# === ĐÃ THÊM === đổi phụ thuộc từ "syscalltbl.lst" sang "src/syscalltbl.lst"
os: $(OBJ) syscalltbl.lst $(OS_OBJ)
	$(MAKE) $(LFLAGS) $(OS_OBJ) -o os $(LIB)

$(OBJ)/%.o: %.c ${HEADER} $(OBJ)
	$(MAKE) $(CFLAGS) $< -o $@

# Prepare objectives container
$(OBJ):
	mkdir -p $(OBJ)

clean:
	rm -f $(SRC)/*.lst
	rm -f $(OBJ)/*.o os sched mem pdg
	rm -rf $(OBJ)
