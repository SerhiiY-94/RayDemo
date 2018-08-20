
import os
import urllib2
import zipfile

def download_file(id, file_name):
    print 'downloading', file_name
    
    url = "https://docs.google.com/uc?export=download&id=" + id

    u = urllib2.urlopen(url)
    f = open(file_name, 'wb')

    file_size_dl = 0
    block_sz = 32768
    while True:
        buffer = u.read(block_sz)
        if not buffer:
            break

        file_size_dl += len(buffer)
        f.write(buffer)
    f.close()

def unzip_file(file_name, unzip_dest):
    print 'extracting', file_name, 'to', unzip_dest
    
    zip_ref = zipfile.ZipFile(file_name, 'r')
    zip_ref.extractall(unzip_dest)
    zip_ref.close()

download_file('189FAfP3qt_UAY1HMF5SolJtvp8DRHnrG', 'libs.zip')
unzip_file('libs.zip', 'src/')
os.remove('libs.zip')
download_file('1Lmfw96dTRpZ2a3Or-DUqzlntcCHsaFaR', 'assets.zip')
unzip_file('assets.zip', '')
os.remove('assets.zip')
