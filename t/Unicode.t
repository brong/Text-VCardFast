# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl Text-VCardFast.t'

#########################

# change 'tests => 1' to 'tests => last_test_to_print';

use strict;
use warnings;

use Test::More tests => 2;
BEGIN { use_ok('Text::VCardFast') };

# http://en.wikipedia.org/wiki/VCard

my @cards = (
'BEGIN:VCARD
VAL:а и м т щ. А И М Т Щ
END:VCARD',
);

my @expected = (
        {
          'objects' => [
                         {
                           'type' => 'VCARD',
                           'properties' => {
                                             'val' => [
                                                        {
                                                          'value' => "\x{430} \x{438} \x{43c} \x{442} \x{449}. \x{410} \x{418} \x{41c} \x{422} \x{429}"
                                                        }
                                                      ]
                                           }
                         }
                       ]
        },
);

foreach my $n (0..$#cards) {
    my $hash = eval { Text::VCardFast::vcard2hash($cards[$n], { multival => ['adr', 'n'] }) };
    is_deeply($hash, $expected[$n]);
}
