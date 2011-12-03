# Message Pack vs similar utilities
# using ruby 1.9.2p290 (2011-07-09 revision 32553) [x86_64-darwin10.8.0]
#
# Packing
# pack: message pack               0.142954
# pack: marshall                   0.636924
# pack: json                       3.001180
# pack: ox                         0.108832
# 
# Unpacking
# unpack: message pack             0.260064
# unpack: marshal                  0.616197
# unpack: marshal                  0.609927
# unpack: ox                       0.287053

#require 'msgpack'
require 'json'
require 'ox'

iter = 100000

duck = { 'sound' => 'quack', 'name' => 'Daffy', 'feet' => 2, 'wings' => true }
dude = { 'sound' => "I'm the dude!", 'name' => 'Lebowski', 'missing a rug' => true, 'missing money' => 1_000_000.00 }
stuff = { :string => "A string", :array => [true, 2, 'yes'], :hash => { :string => "not very deep" }}

def bench(title, iter, &blk)
  start = Time.now
  (1..iter).each { blk.call }
  time = Time.now - start
  puts "%-30s %10.6f" % [title, time]
end

def bench_all(title, iter, obj)
  puts "\n#{title} Packing"
#  bench('pack: message pack', iter) { MessagePack.pack(obj) }
  bench('pack: marshall', iter) { Marshal.dump(obj) }
  bench('pack: json', iter) { JSON.dump(obj) }
  bench('pack: ox', iter) { Ox.dump(obj) }

  puts "\n#{title} Unpacking"
#  mp_obj = MessagePack.pack(obj)
#  bench('unpack: message pack', iter) { MessagePack.unpack(mp_obj) }
  mars_obj = Marshal.dump(obj)
  bench('unpack: marshal', iter) { Marshal.load(mars_obj) }
  json_obj = JSON.dump(obj)
  bench('unpack: json', iter) { JSON.parse(json_obj) }
  ox_obj = Ox.dump(obj)
  bench('unpack: ox', iter) { Ox.parse_obj(ox_obj) }
end

bench_all('duck', iter, duck)
bench_all('dude', iter, dude)
bench_all('stuff', iter, stuff)
