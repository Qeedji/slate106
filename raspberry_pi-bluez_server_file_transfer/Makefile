BLUEZ_DIR:=
BLUEZ_BUILD=no
ifeq ($(BLUEZ_BUILD),yes)
	LIB	:= ../bluez-5.54/lib/.libs/libbluetooth-internal.a ../bluez-5.54/src/.libs/libshared-mainloop.a
	BLUEZ_DIR	:=../bluez-5.54
else
	BLUEZ_DIR	:=bluez_deps/
	LIB	:= bluez_deps/lib/lib/libbluetooth-internal.a bluez_deps/src/lib/libshared-mainloop.a
	
endif
INCLUDE_DIR	:= -Isrc/ -Iinc/ -Iinc/libcrc32/ -Iinc/libe_kermit/ -Iinc/libfile_transfer/ -I$(BLUEZ_DIR)/ -I$(BLUEZ_DIR)/src/shared/  -I$(BLUEZ_DIR)/lib/ -I$(BLUEZ_DIR)/src/ 

CC=gcc
EXEC=bin/bluez_server_file_transfer
LDFLAGS	= $(LIB) -lbluetooth -lpthread

all:$(EXEC)
  
$(EXEC): main.o fifo.o util.o mainloop.o att.o queue.o gatt-db.o gatt-client.o gatt-server.o kermit.o unixio_rpi.o libe-kermit.o libfile_transfer.o libcrc32_file.o uuid.o file_transfer_task.o
	$(CC) -o $@ $^ $(INCLUDE_DIR) $(LDFLAGS) 

main.o : src/main.c
	$(CC) -o $@ -c $< $(INCLUDE_DIR) $(LDFLAGS)
  
fifo.o : src/fifo.c
	$(CC) -o $@ -c $< $(INCLUDE_DIR) $(LDFLAGS)
   
util.o : $(BLUEZ_DIR)/src/shared/util.c
	$(CC) -o $@ -c $< $(INCLUDE_DIR) $(LDFLAGS)
         
mainloop.o : src/mainloop.c 
	$(CC) -o $@ -c $< $(INCLUDE_DIR) $(LDFLAGS) 
             
att.o : $(BLUEZ_DIR)/src/shared/att.c
	$(CC) -o $@ -c $< $(INCLUDE_DIR) $(LDFLAGS)
        
queue.o : $(BLUEZ_DIR)/src/shared/queue.c 
	$(CC) -o $@ -c $< $(INCLUDE_DIR) $(LDFLAGS)

gatt-db.o : $(BLUEZ_DIR)/src/shared/gatt-db.c 
	$(CC) -o $@ -c $< $(INCLUDE_DIR) $(LDFLAGS) 
            
gatt-client.o : $(BLUEZ_DIR)/src/shared/gatt-client.c 
	$(CC) -o $@ -c $< $(INCLUDE_DIR) $(LDFLAGS)
             
gatt-server.o : $(BLUEZ_DIR)/src/shared/gatt-server.c 
	$(CC) -o $@ -c $< $(INCLUDE_DIR) $(LDFLAGS)
                         
kermit.o : src/libe_kermit/kermit.c  
	$(CC) -o $@ -c $< $(INCLUDE_DIR) $(LDFLAGS) 
           
unixio_rpi.o : src/libe_kermit/unixio_rpi.c 
	$(CC) -o $@ -c $< $(INCLUDE_DIR) $(LDFLAGS)

libe-kermit.o : src/libe_kermit/libe-kermit.c 
	$(CC) -o $@ -c $< $(INCLUDE_DIR) $(LDFLAGS)
            
libfile_transfer.o : src/libfile_transfer/libfile_transfer.c
	$(CC) -o $@ -c $< $(INCLUDE_DIR) $(LDFLAGS)
                     
libcrc32_file.o : src/libcrc32/libcrc32_file.c 
	$(CC) -o $@ -c $< $(INCLUDE_DIR) $(LDFLAGS)
                  
uuid.o : $(BLUEZ_DIR)/lib/uuid.c
	$(CC) -o $@ -c $< $(INCLUDE_DIR) $(LDFLAGS)

file_transfer_task.o : src/file_transfer_task.c
	$(CC) -o $@ -c $<  $(INCLUDE_DIR) $(LDFLAGS)
                  
clean:  
	rm -f *.o 
mrproper: clean
	rm -rf $(EXEC)