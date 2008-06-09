#!/usr/bin/env python

'''Upload dist/ files to code.google.com.  For Alex only :-)
'''

__docformat__ = 'restructuredtext'
__version__ = '$Id$'

import os
import sys

base = os.path.dirname(__file__)
dist = os.path.join(base, '../dist')

import googlecode_upload

if __name__ == '__main__':
    password = open(os.path.expanduser('~/.googlecode-passwd')).read().strip()
    version = open(os.path.join(base, '..', 'VERSION')).read().strip()

    descriptions = {}
    for line in open(os.path.join(base, 'descriptions.txt')):
        prefix, description = line.split(' ', 1)
        descriptions[prefix] = description.strip()

    files = {}
    for filename in os.listdir(dist):
        if not os.path.isfile(os.path.join(dist, filename)):
            continue
        if '-%s.' % version not in filename:
            continue
        for prefix in descriptions:
            if filename.startswith(prefix):
                description = descriptions[prefix]
                files[filename] = description
                print filename
                print '   %s' % description

    print 'Ok to upload? [type "y"]'
    if raw_input().strip() != 'y':
        print 'Aborted.'
        sys.exit(1)

    for filename, description in files.items():
        labels = ['Featured']
        if 'linux' in filename:
            labels.append('OpSys-Linux')
        elif 'darwin' in filename:
            labels.append('OpSys-OSX')
        elif 'src' in filename:
            labels.append('Type-Source')
        elif 'win32' in filename:
            labels.append('OpSys-Windows')
        status, reason, url = googlecode_upload.upload(
            os.path.join(dist, filename),
            'avbin',
            'Alex.Holkner',
            password,
            description,
            labels)
        if url:
            print 'OK: %s' % url
        else:
            print 'Error: %s (%s)' % (reason, status)

    print 'Done!'
