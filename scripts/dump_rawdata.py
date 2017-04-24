#!/usr/bin/env python

# Copyright (c) 2016, Intel Corporation. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice,
# this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
# this list of conditions and the following disclaimer in the documentation
# and/or other materials provided with the distribution.
#
# 3. Neither the name of the copyright holder nor the names of its contributors
# may be used to endorse or promote products derived from this software without
# specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.

import os
import sys
import argparse
from struct import *
from subprocess import Popen, PIPE

def find_dfu(package):
    print ("Looking for dfu-util in " + package)
    cmd = "dpkg -L " + package + " 2>/dev/null | grep 'dfu-util' | grep -v txt"
    p = Popen(cmd, stdout=PIPE, stderr=PIPE, shell=True)
    stdout, stderr = p.communicate()
    if stderr:
        return ""
    else:
        #Analyse stdout and check if dfu-util realy exist
        stdout_list = stdout.split('\n')
        if len(stdout_list) == 2:
            dfupath = stdout_list[0]
            if os.path.exists(dfupath) == True:
                print 'dfu-util path is ' + dfupath
                print
                return dfupath
            else:
                print "Wrong dfu-util found"
                print "Check external tool in " + package
                exit(2)
        else:
            return ""

def decode_data_sandbox (data, size, freq, fd):
    timestamp = unpack('<I', data[0:4])[0]
    start = 4
    A = []
    G = []
    while (start < size):
        valtype = unpack('<B', data[start])[0]
        if valtype == 0:
            break
        start = start + 1
        vallen = unpack('<B', data[start])[0]
        if valtype == 1:
            eltsize = 6
        elif valtype == 2:
            eltsize = 12
        else:
            print "Unsupported sensor type %d"%valtype
            start = start + vallen
            continue
        start = start + 1
        for l in range(vallen / eltsize):
            if valtype == 1:
                # ACCEL
                A.append(str(unpack('<h', data[start:start+2])[0]) + ',' + \
                    str(unpack('<h', data[start+2:start+4])[0]) + ',' + \
                    str(unpack('<h', data[start+4:start+6])[0]))
            elif valtype == 2:
                # GYRO
                G.append(str(unpack('<i', data[start:start+4])[0]) + ',' + \
                    str(unpack('<i', data[start+4:start+8])[0]) + ',' + \
                    str(unpack('<i', data[start+8:start+12])[0]))
            else:
                print "Unsupported sensor type %d"%valtype
                break
            start = start + eltsize

    if (len(A) == 0 and len(G) == 0):
        print "ERROR: No Accel and No gyro"
    elif (len(A) == 0):
        for i in range(len(G)):
            fd.write(str(timestamp - ((len(G) - (i + 1)) * (1000 / freq))) + ',,,,' + G[i] + '\n')
    elif (len(G) == 0):
        for i in range(len(A)):
            fd.write(str(timestamp - ((len(A) - (i + 1)) * (1000 / freq))) + ',' + A[i] + ',,,\n')
    elif (len(A) == len(G)):
        for i in range(len(A)):
            fd.write(str(timestamp - ((len(A) - (i + 1)) * (1000 / freq))) + ',' + A[i] + ',' + G[i] + '\n')
    else:
        print "ERROR: %d Accel and %d Gyro samples"%(len(A), len(G))


def decode_data (data, size, fd):
    timestamp = unpack('<I', data[0:4])[0]
    start = 4
    while (start < size):
        valtype = unpack('<B', data[start])[0]
        if valtype == 0:
            break
        start = start + 1
        vallen = unpack('<B', data[start])[0]
        if valtype == 1:
            eltsize = 6
        elif valtype == 2:
            eltsize = 12
        else:
            print "Unsupported sensor type %d"%valtype
            start = start + vallen
            continue
        start = start + 1
        for l in range(vallen / eltsize):
            if valtype == 1:
                # ACCEL
                A = str(unpack('<h', data[start:start+2])[0]) + ';' + \
                    str(unpack('<h', data[start+2:start+4])[0]) + ';' + \
                    str(unpack('<h', data[start+4:start+6])[0])
            elif valtype == 2:
                # GYRO
                A = str(unpack('<i', data[start:start+4])[0]) + ';' + \
                    str(unpack('<i', data[start+4:start+8])[0]) + ';' + \
                    str(unpack('<i', data[start+8:start+12])[0])
            elif valtype == 14:
                # KB
                A = str(unpack('<h', data[start:start+4])[0])
            fd.write(str(timestamp) + ';' + str(valtype) + ';' + A + '\n')
            start = start + eltsize

if __name__ == "__main__":

    # Search dfu-util place
    dfupath = ""
    # Looking in platformflashtoollite
    dfupath = find_dfu("platformflashtoollite")
    if dfupath == "":
        # Looking in platformflashtool
        dfupath = find_dfu("platformflashtool")

    if len(dfupath) == 0:
        print
        print "ERROR -- dfu-util not found"
        print
        exit(2)

    parser = argparse.ArgumentParser()
    parser.add_argument('action', action='store',
                        help='dump or clear user data partition')
    parser.add_argument('-f', '--files_header', action='store', help='files header (dump by default)', default="dump")
    parser.add_argument("-csv", "--csv", help="create csv file",
                    action="store_true")
    parser.add_argument('-freq', '--frequency', action='store',
			help='Sensor sampling rate frequency in Hz (100hz by default)')

    serial_flash_block_size = 4096
    user_data_nb_block = 506
    block_header_size = 12

    # user data partition organization
    # Each header block contains 12 bytes
    # 0 : 1  => MAGIC = 0xABCD
    # 2 : 3  => size of each element in the block
    # 4 : 7  => write pointer status
    # 8 : 11 => read pointer status
    # The pointer status is:
    #    AAAAAAAA the pointer is in this block
    #    00000000 the pointer has visited and left this block
    #    FFFFFFFF the pointer never visited this block
    # Before each element 4 bytes give the status:
    # FFFFFFFF the element is empty
    # BBBBBBBB the element is written
    # 00000000 the element is read

    args = parser.parse_args()
    action = args.action
    print "Param: " + action

    # Files
    files_header = args.files_header
    user_data_partition_file = files_header + '_part.bin'
    useful_user_data_file = files_header + '_data.bin'

    write_pointer = 0
    read_pointer = 0
    elt_size = 0

    if action == 'dump':
        print 'Dumping external flash...'
        res = 0
        os.system('rm -f ' + user_data_partition_file)
        # Retrieve user data partition using dfu-util
        res = os.system(dfupath + ' -a user_data -R -U ' + user_data_partition_file)
        if res != 0:
            print '\n ERROR -- External flash dump has failed'
            exit(1)

        file_read = []
        fd = open(user_data_partition_file, "rb")
        # Find read and write pointers
        for i in range(user_data_nb_block):
            file_read.append(fd.read(serial_flash_block_size))
            if file_read[i][0:2] != "\xCD\xAB":
               file_read.pop()
               fd.close()
               break

            write_pointer_status = file_read[i][4:8]
            read_pointer_status = file_read[i][8:12]
            if ((write_pointer_status == '\xAA\xAA\xAA\xAA') or (read_pointer_status == '\xAA\xAA\xAA\xAA')):

                elt_size = unpack('<H', file_read[i][2:4])[0] + 4
                # Find write pointer or/and read in this block
                for j in range (12, serial_flash_block_size, elt_size):
                    if ((write_pointer_status == '\xAA\xAA\xAA\xAA') and (file_read[i][j:j+4] == '\xFF\xFF\xFF\xFF')):
                        # Write pointer is the first empty element
                        write_pointer = j + (i * serial_flash_block_size)
                        print "write pointer in block %d"%(i+1)
                        print "write pointer found: %X\n"%write_pointer
                        # Write pointer is found, update its status
                        write_pointer_status = 'found'
                    elif ((read_pointer_status == '\xAA\xAA\xAA\xAA') and (file_read[i][j:j+4] == '\xBB\xBB\xBB\xBB')):
                        # Read pointer is the first written element
                        read_pointer = j + (i * serial_flash_block_size)
                        print "read pointer in block %d"%(i+1)
                        print "read pointer found: %X\n"%read_pointer
                        # Read pointer is found, update its status
                        read_pointer_status = 'found'

        if write_pointer != 0 and read_pointer != 0:
            fd = open(user_data_partition_file, "rb")
            file_read = fd.read(serial_flash_block_size * user_data_nb_block)
            os.system('rm -f ' + useful_user_data_file + ' ; touch ' + useful_user_data_file)
            fd2 = open(useful_user_data_file, "wb")
            # Create binary with only useful raw sensor data
            if write_pointer > read_pointer:
                fd2.write(file_read[read_pointer:write_pointer])
            else:
                fd2.write(file_read[read_pointer:-1])
                fd2.write(file_read[0:write_pointer])

            fd.close()
            fd2.close()

            # Read useful rawdata binary file
            size_file = os.stat(useful_user_data_file).st_size
            fd = open(useful_user_data_file, "rb")
            f_read = fd.read(size_file)
            fd.close()

            if args.csv == True:
                frequency = 0

                if args.frequency:
                    frequency = int(args.frequency)
                else:
                    # Default frequency
                    frequency = 100
                decoded_raw_data_file = files_header + '_data.csv'
                # Create csv with decoded raw sensor data
                os.system('rm -f '+ decoded_raw_data_file + '; touch ' + decoded_raw_data_file)
                fd_csv = open(decoded_raw_data_file, "wb")
                fd_csv.write("Timestamp;T;<val>\n")

                decoded_raw_data_file = files_header + '_snor.csv'
                # Create csv with decoded raw sensor data
                os.system('rm -f '+ decoded_raw_data_file + '; touch ' + decoded_raw_data_file)
                fd_csv_sandbox = open(decoded_raw_data_file, "wb")
                fd_csv_sandbox.write("Timestamp,AccelerometerX,AccelerometerY,AccelerometerZ,GyroscopeX,GyroscopeY,GyroscopeZ\n")

            # Decode partition
            offset = 0
            first_timestamp = 0
            last_timestamp = 0
            nb_elements = 0
            elt_size = elt_size - 4

            while (offset < size_file):
                 if (f_read[offset:offset+2] == '\xCD\xAB'):
                     # Move offset to the first element of the current block
                     offset = offset + block_header_size
                 elif (f_read[offset:offset+1] == '\xFF'):
                     # Find next block
                     tmp = f_read[offset:]
                     index_block = tmp.index('\xCD\xAB')
                     if (index_block == -1):
                         print "Next block not found"
                         break
                     else:
                         # Move offset to the first element of the next block
                         offset = offset + index_block + block_header_size

                 if (f_read[offset:offset+4] == '\xBB\xBB\xBB\xBB'):
                     timestamp = unpack('<I', f_read[offset+4:offset+8])[0]
                     if (offset == 0):
                         first_timestamp = timestamp
                     else:
                         last_timestamp = timestamp
                     nb_elements = nb_elements + 1
                     if args.csv == True:
                         decode_data(f_read[offset+4:], elt_size, fd_csv)
                         decode_data_sandbox(f_read[offset+4:], elt_size, frequency, fd_csv_sandbox)
                     # Move offset to next element
                     offset = offset + 4 + elt_size
                 else:
                     print "ERROR: " + f_read[offset:offset+2]
                     break

            print "Number of record in rawdata partition: %d"%(nb_elements)
            print "    First Time stamp: %d"%(first_timestamp)
            print "    First Time stamp: 0x%x"%(first_timestamp)
            print "    Last Time stamp : %d"%(last_timestamp)
            print "    Last Time stamp : 0x%x"%(last_timestamp)
            print "    Duration: %d ms"%(last_timestamp - first_timestamp)

            fd2.close()

        else:
            print '\n ERROR -- pointer not found in user partition:'
            print 'read point: %d'%read_pointer
            print 'write pointer: %d'%write_pointer
            exit(1)

    elif action == 'clear':
        print 'Cleaning external flash...'
        pattern_in=0
        pattern_out=377
        os.system('rm -f erase.bin')
        cmd = 'dd if=/dev/zero ibs=' + str(serial_flash_block_size) + ' count=' + str(user_data_nb_block) + \
              ' | tr "\%d" "\%d" > erase.bin'%(pattern_in, pattern_out)
        os.system(cmd)
        # Clean external flash
        res = os.system(dfupath + ' -a user_data -R -D erase.bin')
        os.system('rm -f erase.bin')
        if res != 0:
            print '\n ERROR -- External flash dump has failed'
            exit(1)
    else:
        print 'Action shall be dump or clear'
