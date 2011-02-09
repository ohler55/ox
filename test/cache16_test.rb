#!/usr/bin/env ruby -wW1

$: << '.'
$: << '../lib'
$: << '../ext'

if __FILE__ == $0
  if (i = ARGV.index('-I'))
    x,path = ARGV.slice!(i, 2)
    $: << path
  end
end

require 'ox'

Ox.cache16_test

