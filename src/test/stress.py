#!/usr/bin/python2.7
print 'Welcome to a python memcached (lsmcd) stress test.'
print 'Find out when entries start getting purged'
print 'Uses bmemcached (pip install python-binary-memcached)'
import bmemcached, argparse, random, sys
parser = argparse.ArgumentParser(description='Stressing memcached (lsmcd)')
parser.add_argument("--u", default="user", type=str, help="SASL user name")
parser.add_argument("--p", default="password", type=str, help="SASL password")
parser.add_argument("--k", default=100, type=int, help="key size")
parser.add_argument("--v", default=10000, type=int, help="value size")
parser.add_argument("--c", default=100, type=int, help="how often to check to see that ALL of the key/values are there")
parser.add_argument("--n", default=0, type=int, help="set to 1 to turn off SASL")
args = parser.parse_args()
user = args.u
password = args.p
ks = args.k
vs = args.v
check = args.c
print 'Connect...'
if args.n == 1:
    print "Connecting without SASL"
    client = bmemcached.Client(('127.0.0.1:11211',),compression=None)
else:
    print "Connecting with SASL"
    client = bmemcached.Client(('127.0.0.1:11211',), user, password, compression=None)
if not client.set('key', 'value'):
    print "Can't even save 1 simple key/value pair - fail"
    sys.exit()
    
if not client.get('key'):
    print "Can't even save and retrieve 1 simple key/value pair - fail"
    sys.exit()
print 'Getting key=' + client.get('key') + ' (should be value)'
if client.get('key') == 'value':
    print 'Initial test works.  Now pound it!'
key_size = 0
data_size = 0
set_index = 0
error = False
key_format = '{0:0' + str(ks) + 'd}'
value_format = '{0:0' + str(vs) + 'd}'
while True:
    set_index = set_index + 1
    key = key_format.format(set_index)
    value = value_format.format(random.randrange(0, vs))
    if (not client.set(key, value)):
        print 'Error in set of key #' + str(set_index) + ' after adding ' + str(key_size) + ' bytes of key and ' + str(data_size) + ' bytes of data'
        break
    key_size = key_size + ks
    data_size = data_size + vs
    if set_index % check == 0:
        print '   Doing check at ' + str(set_index) + ' keys (added ' + str(key_size) + ' bytes of key and ' + str(data_size) + ' bytes of data)'
        for get_index in range(1, set_index):
            key = '{0:0100d}'.format(get_index)
            value = '{0:010000d}'.format(get_index)
            if (not client.get(key)):
                print 'Error in get of key #' + str(get_index) + ' after adding ' + str(key_size) + ' bytes of key and ' + str(data_size) + ' bytes of data'
                error = True
                break
    if error:
        break
print 'Completed stress test'
        
    
    