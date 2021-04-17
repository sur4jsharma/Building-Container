mem_chunk = 5000000
msg = "msg"*mem_chunk

while True:
    msg += "msg"*mem_chunk
    print "%dMB" %(len(msg)/1000000,)
