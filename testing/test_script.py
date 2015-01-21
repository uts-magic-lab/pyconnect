#
#  This simple script demonstrate programming/control of multiple 
#  PyConnect objects.
#
#  test_script.py
#
#  Copyright 2006, 2007 Xun Wang.
#  This file is part of PyConnect.
#
#  PyConnect is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 3 of the License, or
#  (at your option) any later version.
#
#  PyConnect is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

import PyConnect

pTS1 = None
pTS2 = None

def onTimer( tick ):
#  global pTS1, pTS2

  if (tick % 10 == 0):
    print 'tick reached', tick
    pTS1.printThisText( 'tick reached %d' % tick )

  if (tick >= 30):
    print 'disable timer at 30 on TestSample2'
    pTS1.printThisText( 'disable timer at 30 on TestSample2' )
    pTS2.disableTimer()

def onNewObject( obj ):
  global pTS1, pTS2
  print 'got new object', obj.__name__
  if obj.__name__ == 'TestSample1':
    pTS1 = obj
    if (pTS2 != None):
      pTS2.enableTimer( 1000 )
      print 'enable timer on TestSample2'
      pTS1.printThisText( 'enable timer on TestSample2' )
      
  elif obj.__name__ == 'TestSample2':
    pTS2 = obj
    pTS2.ontimerTriggerNoUpdate = onTimer
    if (pTS1 != None):
      print 'enable timer on TestSample2'
      pTS2.enableTimer( 1000 )
      pTS1.printThisText( 'enable timer on TestSample2' )

def onDeadObject( name, id ):
  global pTS1, pTS2
  print 'dead object', name
  if (pTS1 and pTS1.id == id):
    if (pTS2):
      pTS2.disableTimer()
    pTS1 = None
  elif (pTS2 and pTS2.id == id):
    pTS2 = None

def quit():
  global pTS1, pTS2
  if (pTS1):
    pTS1.quit()
  if (pTS2):
    pTS2.quit()

def run():
  global pTS1, pTS2
  if (pTS1 and pTS2):
    pTS2.enableTimer( 1000 )
  else:
    print 'invoke PyConnect discovery'
    PyConnect.discover()


PyConnect.onModuleCreated = onNewObject
PyConnect.onModuleDestroyed = onDeadObject
