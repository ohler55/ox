# Copyright (c) 2011, Peter Ohler<br>
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# - Redistributions of source code must retain the above copyright notice, this
#   list of conditions and the following disclaimer.
#
# - Redistributions in binary form must reproduce the above copyright notice,
#   this list of conditions and the following disclaimer in the documentation
#   and/or other materials provided with the distribution.
#
# - Neither the name of Peter Ohler nor the names of its contributors may be 
#   used to endorse or promote products derived from this software without
#   specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# === Description:
# 
# Ox handles XML documents in two ways. It is a generic XML parser and writer as
# well as a fast Object / XML marshaller. Ox was written for speed as a
# replacement for Nokogiri and for Marshal.
# 
# As an XML parser it is 2 or more times faster than Nokogiri and as a generic
# XML writer it is 14 times faster than Nokogiri. Of course different files may
# result in slightly different times.
# 
# As an Object serializer Ox is 4 times faster than the standard Ruby
# Marshal.dump(). Ox is 3 times faster than Marshal.load().
# 
# === Object Dump Sample:
# 
#   require 'ox'
# 
#   class Sample
#     attr_accessor :a, :b, :c
# 
#     def initialize(a, b, c)
#       @a = a
#       @b = b
#       @c = c
#     end
#   end
# 
#   # Create Object
#   obj = Sample.new(1, "bee", ['x', :y, 7.0])
#   # Now dump the Object to an XML String.
#   xml = Ox.dump(obj)
#   # Convert the object back into a Sample Object.
#   obj2 = Ox.parse_obj(xml)
# 
# === Generic XML Writing and Parsing:
# 
#   require 'ox'
# 
#   doc = Ox::Document.new(:version => '1.0')
# 
#   top = Ox::Element.new('top')
#   top[:name] = 'sample'
#   doc << top
# 
#   mid = Ox::Element.new('middle')
#   mid[:name] = 'second'
#   top << mid
# 
#   bot = Ox::Element.new('bottom')
#   bot[:name] = 'third'
#   mid << bot
# 
#   xml = Ox.dump(doc)
#   puts xml
#   doc2 = Ox.parse(xml)
#   puts "Same? #{doc == doc2}"
module Ox

end

require 'ox/version'
require 'ox/error'
require 'ox/hasattrs'
require 'ox/node'
require 'ox/comment'
require 'ox/instruct'
require 'ox/cdata'
require 'ox/doctype'
require 'ox/element'
require 'ox/document'
require 'ox/bag'
require 'ox/sax'

require 'ox/ox' # C extension
