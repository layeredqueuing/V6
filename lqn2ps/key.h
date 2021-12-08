/* -*- c++ -*-
 * element.h	-- Greg Franks
 *
 * $Id: key.h 15171 2021-12-08 03:02:09Z greg $
 */

#ifndef SRVN2EEPIC_KEY_H
#define SRVN2EEPIC_KEY_H

#include "lqn2ps.h"
#include <vector>
#include "point.h"

class Arc;
class Label;
class Key;

std::ostream& operator<<( std::ostream&, const Key& );

class Key 
{
private:
    Key( const Key& );
    Key& operator=( const Key& );

public:
    Key();
    ~Key();
    Key& init( Point&, Point& ); 

    Key& justification( const Justification justify ) { _justification = justify; return *this; }
    double width() const { return extent.x(); }
    double height() const { return extent.y(); }
    Key& moveTo( const double x, const double y );
    Key& moveBy( const double, const double );
    Key& scaleBy( const double, const double );
    Key& translateY( const double );

    Key& label();

    std::ostream& print( std::ostream& ) const;

private:
    Point origin;
    Point extent;
    Justification _justification;
    std::map<Label *, Arc *> _labels;
};

#endif
