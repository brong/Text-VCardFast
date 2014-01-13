# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl Text-VCardFast.t'

#########################

# change 'tests => 1' to 'tests => last_test_to_print';

use strict;
use warnings;

use Test::More tests => 2;
BEGIN { use_ok('Text::VCardFast') };

my $card = <<EOF;
BEGIN:VCALENDAR
PRODID:-//Google Inc//Google Calendar 70.9054//EN
VERSION:2.0
CALSCALE:GREGORIAN
METHOD:PUBLISH
X-WR-CALNAME:brongondwana\@gmail.com
X-WR-TIMEZONE:Australia/Sydney
BEGIN:VTIMEZONE
TZID:Australia/Sydney
X-LIC-LOCATION:Australia/Sydney
BEGIN:STANDARD
TZOFFSETFROM:+1100
TZOFFSETTO:+1000
TZNAME:EST
DTSTART:19700405T030000
RRULE:FREQ=YEARLY;BYMONTH=4;BYDAY=1SU
END:STANDARD
BEGIN:DAYLIGHT
TZOFFSETFROM:+1000
TZOFFSETTO:+1100
TZNAME:EST
DTSTART:19701004T020000
RRULE:FREQ=YEARLY;BYMONTH=10;BYDAY=1SU
END:DAYLIGHT
END:VTIMEZONE

BEGIN:VEVENT
DTSTART;TZID=Australia/Sydney:20130410T123000
DTEND;TZID=Australia/Sydney:20130410T131500
RRULE:FREQ=WEEKLY;BYDAY=WE;UNTIL=20130508T023000Z
DTSTAMP:20131213T004635Z
UID:3pp269p6ef3qhb12uaab8lkp2c\@google.com
CREATED:20130407T133000Z
DESCRIPTION:
LAST-MODIFIED:20130508T051342Z
LOCATION:
SEQUENCE:1
STATUS:CONFIRMED
SUMMARY:Step Bourke St
TRANSP:OPAQUE
END:VEVENT

END:VCALENDAR
EOF
my $hash = Text::VCardFast::vcard2hash($card, {multival => ['rrule']});

use Data::Dumper;
$Data::Dumper::Indent = 1;
die Dumper($hash);

#########################

# Insert your test code below, the Test::More module is use()ed here so read
# its man page ( perldoc Test::More ) for help writing this test script.

