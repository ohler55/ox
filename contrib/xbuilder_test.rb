#!/usr/bin/env ruby
# Contributed by Notezen

$: << File.join(File.dirname(__FILE__), '../lib')
$: << File.join(File.dirname(__FILE__), '../ext')
$: << File.join(File.dirname(__FILE__), '.')

require 'test/unit'
require 'ox'
require 'xbuilder'

class XBuilderTest < Test::Unit::TestCase
  include Ox::XBuilder

  def test_build
    xml =
      '<parent>
  <child1 atr1="atr1_value" atr2="atr2_value">
    <subchild1>some text</subchild1>
    <subchild2 atr3="atr3_value">
      <sometag/>
    </subchild2>
  </child1>
  <child3/>
  <child4 atr4="atr4_value">another text</child4>
</parent>'

    children =
      [
        x_if(true, 'child3'),
        x('child4', 'another text', 'atr4' => 'atr4_value'),
        x_if(false, 'child5')
      ]

    n = x('parent',
          x('child1', {'atr1' => 'atr1_value', :atr2 => 'atr2_value'},
            x('subchild1', 'some text'),
            x('subchild2', {'atr3' => 'default_value'}, {'atr3' => 'atr3_value'},
              x('sometag'),
              x_if(false, 'never'))),
          children)

    assert_equal(Ox.dump(n), Ox.dump(Ox.parse(xml)))
  end
end
