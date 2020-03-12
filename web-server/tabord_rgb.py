# add this file to cron:
#  crontab -u www-data -l > /tmp/tabord_rgb-crontab.tmp
#  echo '*/3 * * * * python /full/path/to/tabord_rgb.py >> /dev/null 2>&1' >> /tmp/tabord_rgb-crontab.tmp
#  crontab -u www-data /tmp/tabord_rgb-crontab.tmp
#  crontab -u www-data -l

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
execfile("{cwd}/tabord_settings.py".format(cwd=os.path.dirname(os.path.abspath(__file__))))
g_debug = False # show additional information (debug mode)
g_utc_offset = (datetime.fromtimestamp(1288483950000*1e-3) - datetime.utcfromtimestamp(1288483950000*1e-3)).total_seconds() # don't need tzlocal
g_cache = {'lasttime':0,'kills':[],'losses':[],'npc':[]}


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
        #headers=[{'Content-Type': 'application/json'},{'Accept-Encoding': 'gzip'},{'Maintainer': g_maintainer}]
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
    if g_debug:
        print(req.header_items())
        print('---')
        print(f.info())
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

def readCache(ts):
    fname = '{dir}/tabord_rgb_cache.json'.format(dir=g_directory)
    if os.path.isfile(fname):
        f = open(fname,"rt")
        s = f.read()
        json_data = (json.loads(s))
        f.close()
        return json_data
    return {}

def updateCache(ts):
    fname = '{dir}/tabord_rgb_cache.json'.format(dir=g_directory)
    g_cache["lasttime"] = ts
    s = json.dumps(g_cache, indent=1, sort_keys=False)
    f = open(fname,"wt+")
    f.write(s)
    f.close()
    return

last_eve_time = time.time() - g_utc_offset - 86400 - 300
last_hour_utc = datetime.fromtimestamp(last_eve_time).strftime("%Y%m%d%H00")

cache = readCache(last_eve_time)
if 'lasttime' in cache:
    g_cache = cache

#debug:g_cache = {'lasttime':0,'kills':[81747916,81754564,81754593],'losses':[81749231],'npc':[81753546]}
#last_hour_utc = "202002180000"

# see https://github.com/zKillboard/zKillboard/wiki/API-(Killmails)
alliance_kills = getJson(1,'allianceID/{alliance}/startTime/{time}/kills'.format(alliance=g_alliance_id,time=last_hour_utc))
alliance_looses = getJson(1,'allianceID/{alliance}/startTime/{time}/losses'.format(alliance=g_alliance_id,time=last_hour_utc))

for k in alliance_kills:
    id = int(k["killmail_id"])
    if not id in g_cache["kills"]:
        g_cache["kills"].append(id)

for l in alliance_looses:
    id = int(l["killmail_id"])
    npc = False
    if 'npc' in l["zkb"]:
        if l["zkb"]["npc"]:
            npc = True
    if npc and (not id in g_cache["npc"]):
        g_cache["npc"].append(id)
    if not npc and (not id in g_cache["losses"]):
        g_cache["losses"].append(id)

# combining all killmails
alliance_killmails = list(g_cache["kills"] + g_cache["npc"] + g_cache["losses"])
# sorting all killmails
alliance_killmails.sort(reverse=True)
if g_debug:
    print (alliance_killmails)
# truncating all killmails up to 10 items
del alliance_killmails[10:]
if g_debug:
    print(alliance_killmails)

alliance_killmails_set = set(alliance_killmails)
g_cache["kills"] = list(alliance_killmails_set.intersection(set(g_cache["kills"])))
g_cache["npc"] = list(alliance_killmails_set.intersection(set(g_cache["npc"])))
g_cache["losses"] = list(alliance_killmails_set.intersection(set(g_cache["losses"])))

if g_debug:
    g_cache["kills"].sort()
    g_cache["npc"].sort()
    g_cache["losses"].sort()

result = ''
for k in alliance_killmails:
    if result:
        result = result + ' '
    if k in g_cache["kills"]:
        result = result + '65280' # 0x00ff00
    else:
        result = result + '16711680' # 0xff0000
if g_debug:
    print (result)

fname = '{dir}/{serial_num}.txt'.format(dir=g_directory,serial_num=g_character_id)
f = open(fname,"wt+")
f.write(result)
f.close()

updateCache(last_eve_time)

