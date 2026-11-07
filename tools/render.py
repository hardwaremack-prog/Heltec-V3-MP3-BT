from PIL import Image, ImageDraw, ImageFilter, ImageFont
import glob, os
S=5
B='/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf'
def load(p):
    t=open(p).read().split()[3:]
    return [[t[y*128+x]=='1' for x in range(128)] for y in range(64)]
def oled(px):
    im=Image.new('RGB',(128*S,64*S),(4,6,10)); d=ImageDraw.Draw(im)
    for y in range(64):
        for x in range(128):
            d.rectangle([x*S,y*S,x*S+S-2,y*S+S-2],fill=(225,240,255) if px[y][x] else (10,13,18))
    glow=im.filter(ImageFilter.GaussianBlur(4))
    return Image.blend(im,Image.eval(glow,lambda v:min(255,int(v*1.6))),0.25)
def bezel(sc):
    pad=26; b=Image.new('RGB',(sc.width+pad*2,sc.height+pad*2),(11,15,20)); d=ImageDraw.Draw(b)
    d.rounded_rectangle([2,2,b.width-3,b.height-3],18,fill=(22,27,34),outline=(48,58,70),width=2)
    d.rectangle([pad-6,pad-6,pad+sc.width+5,pad+sc.height+5],fill=(0,0,0)); b.paste(sc,(pad,pad)); return b
names={'01_boot':'Boot','02_home':'Home menu','03_browse':'Browsing a folder','04_now_playing':'Now Playing','05_paused':'Paused',
'06_controls':'Controls menu (hold PRG)','07_bluetooth_scan':'Bluetooth: scanning','08_bluetooth_connecting':'Bluetooth: connecting',
'09_bluetooth_connected':'Bluetooth: headset connected','10_wifi':'Wi-Fi: on home network','11_wifi_hotspot':'Wi-Fi: hotspot mode',
'12_settings':'Settings','13_prg_hold_meter':'PRG hold meter (bottom bar)','14_no_sd':'No SD card','15_about':'About'}
F=ImageFont.truetype(B,22); tiles=[]
for p in sorted(glob.glob('out/*.pbm')):
    k=os.path.basename(p)[:-4]; b=bezel(oled(load(p))); b.save(f'png/{k}.png')
    t=Image.new('RGB',(b.width,b.height+44),(11,15,20)); t.paste(b,(0,44))
    ImageDraw.Draw(t).text((8,10),names[k],font=F,fill=(127,227,255)); tiles.append(t)
cols=3; w,h=tiles[0].size; g=24
sheet=Image.new('RGB',(cols*w+(cols+1)*g,5*h+6*g+70),(11,15,20)); d=ImageDraw.Draw(sheet)
d.text((g,22),'Heltec MP3 v1.1.0 — OLED screens (rendered with the real U8g2 fonts)',font=ImageFont.truetype(B,28),fill=(255,255,255))
for i,t in enumerate(tiles): sheet.paste(t,(g+(i%cols)*(w+g),70+g+(i//cols)*(h+g)))
sheet.save('png/OLED-screens.png')
