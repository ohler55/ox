#!/usr/bin/env ruby

$: << '.'
$: << '../lib'
$: << '../ext'

if __FILE__ == $0 && (i = ARGV.index('-I'))
  x, path = ARGV.slice!(i, 2)
  $: << path
end

require 'ox'

Ox.cache_test
