import struct
import gio

SIZE_ERROR = -1
SEEK_ERROR = -2

def hash_file (name):
    """ FIXME Need to handle exceptions !! """

    longlongformat = 'q' # long long
    bytesize = struct.calcsize (longlongformat)

    file_to_hash = gio.File (name)

    file_info = file_to_hash.query_info ('standard::size', 0)
    filesize = file_info.get_attribute_uint64 ('standard::size')

    file_hash = filesize

    if filesize < 65536 * 2:
        return SIZE_ERROR, 0

    data = file_to_hash.read ()

    if data.can_seek () != True:
        return SEEK_ERROR, 0

    for _ in range (65536/bytesize):
        buf = data.read (bytesize)
        (l_value,) = struct.unpack (longlongformat, buf)
        file_hash += l_value
        file_hash = file_hash & 0xFFFFFFFFFFFFFFFF #to remain as 64bit number

    if data.seek (max (0, filesize-65536), 1) != True:
        return SEEK_ERROR, 0

    for _ in range (65536/bytesize):
        buf = data.read (bytesize)
        (l_value,) = struct.unpack (longlongformat, buf)
        file_hash += l_value
        file_hash = file_hash & 0xFFFFFFFFFFFFFFFF

    data.close ()
    returnedhash = "%016x" % file_hash
    return returnedhash, filesize

