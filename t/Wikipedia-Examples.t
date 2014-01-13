# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl Text-VCardFast.t'

#########################

# change 'tests => 1' to 'tests => last_test_to_print';

use strict;
use warnings;

use Test::More tests => 4;
BEGIN { use_ok('Text::VCardFast') };

# http://en.wikipedia.org/wiki/VCard

my @cards = (
'BEGIN:VCARD
VERSION:2.1
N:Gump;Forrest
FN:Forrest Gump
ORG:Bubba Gump Shrimp Co.
TITLE:Shrimp Man
PHOTO;GIF:http://www.example.com/dir_photos/my_photo.gif
TEL;WORK;VOICE:(111) 555-1212
TEL;HOME;VOICE:(404) 555-1212
ADR;WORK:;;100 Waters Edge;Baytown;LA;30314;United States of America
LABEL;WORK;ENCODING=QUOTED-PRINTABLE:100 Waters Edge=0D=0ABaytown, LA 30314=0D=0AUnited States of America
ADR;HOME:;;42 Plantation St.;Baytown;LA;30314;United States of America
LABEL;HOME;ENCODING=QUOTED-PRINTABLE:42 Plantation St.=0D=0ABaytown, LA 30314=0D=0AUnited States of America
EMAIL;PREF;INTERNET:forrestgump@example.com
REV:20080424T195243Z
END:VCARD',
'BEGIN:VCARD
VERSION:3.0
N:Gump;Forrest;Mr.
FN:Forrest Gump
ORG:Bubba Gump Shrimp Co.
TITLE:Shrimp Man
PHOTO;VALUE=URL;TYPE=GIF:http://www.example.com/dir_photos/my_photo.gif
TEL;TYPE=WORK,VOICE:(111) 555-1212
TEL;TYPE=HOME,VOICE:(404) 555-1212
ADR;TYPE=WORK:;;100 Waters Edge;Baytown;LA;30314;United States of America
LABEL;TYPE=WORK:100 Waters Edge\nBaytown, LA 30314\nUnited States of America
ADR;TYPE=HOME:;;42 Plantation St.;Baytown;LA;30314;United States of America
LABEL;TYPE=HOME:42 Plantation St.\nBaytown, LA 30314\nUnited States of America
EMAIL;TYPE=PREF,INTERNET:forrestgump@example.com
REV:2008-04-24T19:52:43Z
END:VCARD',
'BEGIN:VCARD
VERSION:4.0
N:Gump;Forrest;;;
FN:Forrest Gump
ORG:Bubba Gump Shrimp Co.
TITLE:Shrimp Man
PHOTO;MEDIATYPE=image/gif:http://www.example.com/dir_photos/my_photo.gif
TEL;TYPE=work,voice;VALUE=uri:tel:+1-111-555-1212
TEL;TYPE=home,voice;VALUE=uri:tel:+1-404-555-1212
ADR;TYPE=work;LABEL="100 Waters Edge\nBaytown, LA 30314\nUnited States of America"
  :;;100 Waters Edge;Baytown;LA;30314;United States of America
ADR;TYPE=home;LABEL="42 Plantation St.\nBaytown, LA 30314\nUnited States of America"
 :;;42 Plantation St.;Baytown;LA;30314;United States of America
EMAIL:forrestgump@example.com
REV:20080424T195243Z
END:VCARD'
);

my @expected = (
    {
          'objects' => [
                         {
                           'type' => 'VCARD',
                           'properties' => {
                                             'n' => [
                                                      {
                                                        'value' => [
                                                                     'Gump',
                                                                     'Forrest'
                                                                   ]
                                                      }
                                                    ],
                                             'org' => [
                                                        {
                                                          'value' => 'Bubba Gump Shrimp Co.'
                                                        }
                                                      ],
                                             'photo' => [
                                                          {
                                                            'value' => 'http://www.example.com/dir_photos/my_photo.gif',
                                                            'param' => {
                                                                         'gif' => ''
                                                                       }
                                                          }
                                                        ],
                                             'version' => [
                                                            {
                                                              'value' => '2.1'
                                                            }
                                                          ],
                                             'email' => [
                                                          {
                                                            'value' => 'forrestgump@example.com',
                                                            'param' => {
                                                                         'internet' => '',
                                                                         'pref' => ''
                                                                       }
                                                          }
                                                        ],
                                             'tel' => [
                                                        {
                                                          'value' => '(111) 555-1212',
                                                          'param' => {
                                                                       'voice' => '',
                                                                       'work' => ''
                                                                     }
                                                        },
                                                        {
                                                          'value' => '(404) 555-1212',
                                                          'param' => {
                                                                       'voice' => '',
                                                                       'home' => ''
                                                                     }
                                                        }
                                                      ],
                                             'rev' => [
                                                        {
                                                          'value' => '20080424T195243Z'
                                                        }
                                                      ],
                                             'label' => [
                                                          {
                                                            'value' => '100 Waters Edge=0D=0ABaytown, LA 30314=0D=0AUnited States of America',
                                                            'param' => {
                                                                         'work' => '',
                                                                         'encoding' => 'QUOTED-PRINTABLE'
                                                                       }
                                                          },
                                                          {
                                                            'value' => '42 Plantation St.=0D=0ABaytown, LA 30314=0D=0AUnited States of America',
                                                            'param' => {
                                                                         'home' => '',
                                                                         'encoding' => 'QUOTED-PRINTABLE'
                                                                       }
                                                          }
                                                        ],
                                             'title' => [
                                                          {
                                                            'value' => 'Shrimp Man'
                                                          }
                                                        ],
                                             'adr' => [
                                                        {
                                                          'value' => [
                                                                       '',
                                                                       '',
                                                                       '100 Waters Edge',
                                                                       'Baytown',
                                                                       'LA',
                                                                       '30314',
                                                                       'United States of America'
                                                                     ],
                                                          'param' => {
                                                                       'work' => ''
                                                                     }
                                                        },
                                                        {
                                                          'value' => [
                                                                       '',
                                                                       '',
                                                                       '42 Plantation St.',
                                                                       'Baytown',
                                                                       'LA',
                                                                       '30314',
                                                                       'United States of America'
                                                                     ],
                                                          'param' => {
                                                                       'home' => ''
                                                                     }
                                                        }
                                                      ],
                                             'fn' => [
                                                       {
                                                         'value' => 'Forrest Gump'
                                                       }
                                                     ]
                                           }
                         }
                       ]
    },
	{}, {}
);

foreach my $n (0..$#cards) {
    my $hash = Text::VCardFast::vcard2hash($cards[$n], { multival => ['adr', 'n'] });
    is_deeply($hash, $expected[$n]);
    use Data::Dumper;
    die Dumper($hash);
}
