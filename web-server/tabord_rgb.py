import urllib2
import urllib
import json
import gzip
import sys
import os
from StringIO import StringIO
import time
from datetime import datetime

# Global settings
g_debug = True # show additional information (debug mode)
g_eveserver = 'tranquility' # eveonline' server
g_user_agent = 'http://gf-company.space/ Maintainer: Qandra Si qandra.si@gmail.com'
g_alliance_id = 99008697 # eveonline' alliance to generate video with team work

# type=0 : https://esi.evetech.net/
# type=1 : https://zkillboard.com/
def getJson(type,suburl):
    if type==0:
        url = 'https://esi.evetech.net/latest/{esi}/?datasource={server}'.format(esi=suburl,server=g_eveserver)
        #headers={'Content-Type': 'application/json'}
        #req.addheaders = [('Content-Type', 'application/json')]
    else:
        url = 'https://zkillboard.com/api/{api}/'.format(api=suburl)
        #req.addheaders = [('Content-Type', 'application/json')]
        #headers=[{'Content-Type': 'application/json'},{'Accept-Encoding': 'gzip'},{'Maintainer': 'Alexander alexander.bsrgin@yandex.ru'}]
    if g_debug:
        print(url)
        sys.stdout.flush()
    req = urllib2.Request(url)
    if type==0:
        req.add_header('Content-Type', 'application/json')
    else:
        req.add_header('Accept-Encoding', 'gzip')
        req.add_header('User-Agent', g_user_agent)
    f = urllib2.urlopen(req)
    if f.info().get('Content-Encoding') == 'gzip':
        buffer = StringIO(f.read())
        deflatedContent = gzip.GzipFile(fileobj=buffer)
        s = deflatedContent.read()
    else:
        s = f.read().decode('utf-8')
    f.close()
    json_data = (json.loads(s))
    #if g_debug:
    #    print(s)
    #    print(json.dumps(json_data, indent=4, sort_keys=True))
    time.sleep(2) # Do not hammer the server with API requests. Be polite.
    return json_data

last_hour_utc = datetime.utcnow().strftime("%Y%m%d%H00")
alliance_kills = getJson(1,'allianceID/{alliance}/startTime/{time}/kills'.format(alliance=g_alliance_id,time=last_hour_utc))
alliance_looses = getJson(1,'allianceID/{alliance}/startTime/{time}/losses'.format(alliance=g_alliance_id,time=last_hour_utc))

for k in alliance_kills:
    print('{} '.format(k["killmail_id"]))
for l in alliance_looses:
    print('{} '.format(l["killmail_id"]))
