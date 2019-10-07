# This is the python code for EASY
# It reads from the successful Boogie code and simplify the arbitration in original LLVM IR
# Written by Jianyi Cheng

from __future__ import print_function
import datetime

# this is a function to search if the current arbiter port is in removing list
def search(funcAddr, indexAddr, parIndex):
    j = 0
    while j < len(thd):
        if funcAddr == thd[j] and indexAddr == var[j] and parIndex == bnk[j]:
            break
        j += 1
    return j

# 1.0 Read from Boogie file to generate list of arbiter ports to be removed
thd = []
var = []
bnk = []

op_buffer = []
ctr = 0
err = 0

inBoogie = open("op.bpl")
print("\n******************************************************************")
print("                 Reading from Boogie output...")
print("******************************************************************")
for line in inBoogie:
  if line.startswith("\tassert !t"):
    for s in line.split():
      num_idx = s.find("_index_")
      if num_idx != -1:
        thd.append(int(s[1:num_idx]))
        var.append(int(s[num_idx+7:len(s)]))
      par_idx = s.find("bv32")
      if par_idx != -1:
        bnk.append(int(s[0:par_idx]))
    ctr += 1

#f.write("// This is the Boogie results indicating the arbiter ports that are never used during the execution.\n")
#f.write("// Used by EASY pass to simplify the hardware.\n")
#now = datetime.datetime.now()
#f.write("// "+str(now)+"\n\n")

#f.write("#define NEASY_PORTS "+str(ctr+1)+"\n\n")
#f.write("int thd["+str(ctr+1)+"] = {")
#for i in thd[:-1]:
#    f.write(str(i)+", ")
#f.write(str(thd[-1])+"};\n")

#f.write("int var["+str(ctr+1)+"] = {")
#for i in var[:-1]:
#    f.write(str(i)+", ")
#f.write(str(var[-1])+"};\n")

#f.write("int bnk["+str(ctr+1)+"] = {")
#for i in bnk[:-1]:
#    f.write(str(i)+", ")
#f.write(str(bnk[-1])+"};\n")

#print("find " + str(ctr+1) + " arbiter ports to be removed.")

print("Found " + str(ctr) + " arbiter ports to be removed: \n")
print("\tThread\t=", end = " ")
for i in thd:
    print(str(i), end = " ")
print("\n\tIndex\t=", end = " ")
for i in var:
    print(str(i), end = " ")
print("\n\tBank\t=", end = " ")

for i in bnk:
    print(str(i), end = " ")
print("\n\n")

inBoogie.close()

#open llvm ir code to do code transformation
nopInstr = " = add i8 0, 0\n"
inLLVM = open("input.ll")
f = open("output.ll", "w")

print("\n******************************************************************")
print("                 Checking essential features...")
print("******************************************************************")

# find thread function name, thread number, MUX number and partition number
inLLVM.seek(0)
gFlag = 0   # previous is a getelementptr function of sub-array
indexNum = 0 # number of memory MUX in one thread
funcAddr = -1
nFlag = 0   # name of thread function found
threadNum = 0 # number of threads
partition = []
arrayName = []
# threads
for line in inLLVM:
    addrPFuncHeader = line.find("original_name")
    if addrPFuncHeader != -1:
        threadNum += 1
        if nFlag == 0:
            addrPFuncName = line.find("\"", addrPFuncHeader+16)
            funcName = line[addrPFuncHeader+16 : addrPFuncName]
            nFlag = 1
# MUX
inLLVM.seek(0)
for line in inLLVM:
    hasFuncName = line.find(funcName + "_")
    isDefinition = line.find("define")
    if hasFuncName != -1 and isDefinition != -1:
        funcAddr += 1
    if line.startswith("  %GEP_sub_") and funcAddr == 0:
        # check total number of MUX
        if gFlag == 0:
            indexNum += 1
        gFlag = 1
    else:
        gFlag = 0
# partitions
inLLVM.seek(0)
for line in inLLVM:
    if line.startswith("@sub_"):
        addrAssign = line.find(" = ")
        endAddr = addrAssign-1
        while line[endAddr] != "_":
            endAddr -= 1
        if len(arrayName) == 0:
            arrayName.append(line[1:endAddr])
            partition.append(1)
        else:
            i = 0;
            findOld = 0;
            while i < len(arrayName):
                if arrayName[i] == line[1:endAddr]:
                    partition[i] += 1
                    findOld = 1
                    break
                i += 1
                if findOld == 0:
                    arrayName.append(line[1:endAddr])
                    partition.append(1)
if threadNum != max(thd)+1:
    err += 1
    print("Error: Thread number does not match: has " + str(max(thd)+1) + " in Boogie and " + str(threadNum) + " in LLVM. \n")
if indexNum != max(var)+1:
    err += 1
    print("Error: MUX number does not match: has " + str(max(var)+1) + " in Boogie and " + str(indexNum) + " in LLVM. \n")

print("\nFound thread function name = " + funcName + "\n")
print("Found thread number = " + str(threadNum) + "\n")
i = 0
while i < len(arrayName):
    print("Found partitioned array: " + arrayName[i] + ", partition = " + str(partition[i]) + "\n")
    i += 1

# analyze input code
print("\n******************************************************************")
print("                 Analyzing LLVM input...")
print("******************************************************************\n")

# load input code
inLLVM.seek(0)
for line in inLLVM:
    op_buffer.append(line)

process = [0] * len(op_buffer)
valid = [0] * len(thd)

gFlag = 0   # previous is a getelementptr function of sub-array
indexAddr = -1 # number of memory MUX in one thread
funcAddr = -1
i = 0

# analyze getelementptr instruction
inLLVM.seek(0)
for line in inLLVM:
    hasFuncName = line.find(funcName + "_")
    isDefinition = line.find("define")
    if hasFuncName != -1 and isDefinition != -1:
        funcAddr += 1
        indexAddr = -1
    
    if line.startswith("  %GEP_sub_"):
        # check total number of MUX
        if gFlag == 0:
            indexAddr += 1
        gFlag = 1
        
        # start processing
        addrAssign = line.find(" = ")
        endAddr = addrAssign-1
        while line[endAddr] != "_":
            endAddr -= 1
        startAddr = endAddr - 1
        while line[startAddr] != "_":
            startAddr -= 1
        startAddr += 1
        parIndex = int(line[startAddr:endAddr])
        
        j = search(funcAddr, indexAddr, parIndex)
        if j != len(thd):
            valid[j] = 1
            process[i] = 1      # getelementptr
    else:
        gFlag = 0
    i += 1

if sum(valid) != len(thd):
    err += 1
    print("Error: " + str(len(thd) - sum(valid)) + "arbiter ports in removing list missed. (getelementptr)\n")

print("Extracted getelementptr instructions to be removed.\n ")

valid = [0] * len(thd)
lFlag1 = 0   # previous is a load function of sub-array
lFlag2 = 0   # previous is a load function of sub-array
indexAddr = -1 # number of memory MUX in one thread
funcAddr = -1
i = 0
icmpList = []
orList = []
listFunc = []

# analyze Load instruction
inLLVM.seek(0)
for line in inLLVM:
    hasFuncName = line.find(funcName + "_")
    isDefinition = line.find("define")
    if hasFuncName != -1 and isDefinition != -1:
        funcAddr += 1
        indexAddr = -1
    
    if line.startswith("  %Load_sub_"): # or line.startswith("  %Store_sub_"):
        # check total number of MUX
        if lFlag2 == 0:
            indexAddr += 1

        # start processing
        addrAssign = line.find(" = ")
        endAddr = addrAssign-1
        while line[endAddr] != "_":
            endAddr -= 1
        startAddr = endAddr - 1
        while line[startAddr] != "_":
            startAddr -= 1
        startAddr += 1
        parIndex = int(line[startAddr:endAddr])
        
        j = search(funcAddr, indexAddr, parIndex)
        if j != len(thd):
            valid[j] = 1
            process[i] = 1      # load/store
            process[i+1] = 3    # select

        lFlag2 = lFlag1
        lFlag1 = 1
    elif process[i] == 3:               # i.e. select instruction
        addrAssign = line.find(" = ")
        startAddr = line.find("%")
        i1Addr = line.find("i1")
        commaAddr = line.find(", ")
        orList.append(line[startAddr:addrAssign])
        icmpList.append(line[i1Addr+3:commaAddr])
        listFunc.append(funcAddr)
        lFlag2 = lFlag1
        lFlag1 = 0
    else:
        lFlag2 = lFlag1
        lFlag1 = 0
    i += 1

if sum(valid) != len(thd):
    err += 1
    print("Error: " + str(len(thd) - sum(valid)) + " arbiter ports in removing list missed. (load + select)\n")

print("Extracted load + select instructions to be removed.\n ")

i = 0
print("\nFound icmp var: ")
while i < len(icmpList):
    print(icmpList[i] + ", ", end =" ")
    i += 1
i = 0
print("\nFound or var: ")
while i < len(orList):
    print(orList[i] + ", ", end =" ")
    i += 1
print("\n")

# analyze input code
print("\n******************************************************************")
print("                 Transforming LLVM instructions...")
print("******************************************************************\n")


# analyze predication metadata
i = 0
inLLVM.seek(0)
funcAddr = -1
for line in inLLVM:
    hasFuncName = line.find(funcName + "_")
    isDefinition = line.find("define")
    if hasFuncName != -1 and isDefinition != -1:
        funcAddr += 1
    
    if line.startswith("  %Load_sub_"):
        if process[i] != 1:
            newLoad = line.find(", !legup.ap_pred")
            op_buffer[i] = line[0:newLoad] + ";" + line[newLoad:len(line)]
    elif line.find("= icmp eq") != -1:
        #print(line)
        addrAssign = line.find(" = ")
        startAddr = line.find("%")
        j = 0
        while j < len(icmpList):
            if line[startAddr:addrAssign] == icmpList[j] and funcAddr == listFunc[j]:
                process[i] = 3
            j += 1
        if process[i] != 3:
            newIcmp = line.find(", !legup.ap_pred_id")
            if newIcmp != -1:
                op_buffer[i] = line[0:newIcmp] + ";" + line[newIcmp:len(line)]
    i += 1

print("Transformed load + icmp instruction (including predicate metadata)\n ")

i = 0
inLLVM.seek(0)
funcAddr = -1
for line in inLLVM:
    hasFuncName = line.find(funcName + "_")
    isDefinition = line.find("define")
    if hasFuncName != -1 and isDefinition != -1:
        funcAddr += 1
    
    if line.startswith("  call void @__legup_preserve_value_"):
        if process[i-1] == 0:
            newPreserve = line.find(", !legup.ap_pred_id")
            if newPreserve != -1:
                op_buffer[i] = line[0:newPreserve] + ";" + line[newPreserve:len(line)]
        elif process[i-1] == 3:
            process[i] = 1
        else:
            err += 1
            print("Error: @__legup_preserve_value in wrong order - ")
            print(op_buffer[i-1])
            print(op_buffer[i])
    elif line.find(" = or ") != -1:
        addrAssign = line.find(" = ")
        var1Addr = line.find("%", addrAssign)
        commaAddr = line.find(",")
        var2Addr = line.find("%", commaAddr)
        j = 0
        var1 = 0
        var2 = 0
        while j < len(orList):
            if line[var1Addr:commaAddr] == orList[j] and funcAddr == listFunc[j]:
                var1 = 1
            if line[var2Addr:len(line)-1] == orList[j] and funcAddr == listFunc[j]:
                var2 = 1
            j += 1
        if var1 == 1 and var2 == 1:
            op_buffer[i] = line[0:var1Addr] + "0, 0\n"
        elif var1 == 1:
            op_buffer[i] = line[0:var1Addr] + "0" + line[commaAddr:len(line)]
        elif var2 == 1:
            op_buffer[i] = line[0:var2Addr] + "0\n"
    i += 1
print("Transformed @__legup_preserve_value + Or instruction (including predicate metadata)\n ")



inLLVM.seek(0)
i = 0
for line in inLLVM:
    if process[i] == 1 or process[i] == 2: # instruction to be removed
        f.write(";" + op_buffer[i])
    elif process[i] == 3:       # instruction to be replaced with add
        addrAssign = line.find(" = ")
        startAddr = line.find("%")
        f.write(";" + op_buffer[i])
        f.write("\t" + line[startAddr:addrAssign] + nopInstr)
    elif process[i] == 0:
        f.write(op_buffer[i])
        #isIcmp
    else:
        err += 1
        print("Error: Unkown process code found: " + str(process[i]))
    i += 1

print("Code transformation finished.\n\n")

f.close()
inLLVM.close()

if err == 0:
    print("Transformation success! Error: " + str(err) +"\n")
else:
    print("Transformation failed! Error: " + str(err) +"\n")
