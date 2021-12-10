/* -*- c++ -*-  
 * node.h	-- Greg Franks
 *
 * $Id: node.h 15180 2021-12-08 20:56:48Z greg $
 */

#ifndef _NODE_H
#define _NODE_H

#include <lqn2ps.h>
#include "point.h"
#include "graphic.h"

class Node : public Graphic
{
public:
    typedef Node * (*create_func)( double x1, double y1, double x2, double y2 );
    
    Node() : Graphic(), origin(0,0), extent(0,0) {}
    Node( const Point& aPoint ) : Graphic(), origin(aPoint), extent(0,0) {}
    Node(double x1, double y1, double x2, double y2 ) : Graphic(), origin(x1,y1), extent(x2,y2) {}

    static Node * newNode( double x, double y );

    const Point& getOrigin() const { return origin; }
    const Point& getExtent() const { return extent; }
    Point topLeft() const { return Point( origin.x(), origin.y() + extent.y() ); }
    Point topRight() const { return Point( origin.x() + extent.x(), origin.y() + extent.y() ); }
    Point bottomLeft() const { return Point( origin.x(), origin.y() ); }
    Point bottomRight() const { return Point( origin.x() + extent.x(), origin.y() ); }
    Point center() const { return Point( origin.x() + extent.x()/2, origin.y() + extent.y()/2); }
    Point topCenter() const { return Point( origin.x() + extent.x()/2, origin.y() + extent.y()); }
    Point bottomCenter() const { return Point( origin.x() + extent.x()/2, origin.y()); }
    double left() const { return origin.x(); }
    double right() const { return origin.x() + extent.x(); }
    double top() const { return origin.y() + extent.y(); }
    double bottom() const { return origin.y(); }
    double width() const { return extent.x(); }
    double height() const { return extent.y(); }
    Node& setWidth( const double w ) { extent.x(w); return *this; }
    Node& setHeight( const double h ) { extent.y(h); return *this; }

    /* Computation */

    virtual Node& moveTo( const double x, const double y ) { origin.moveTo( x, y ); return *this; }
    virtual Node& moveTo( const Point& aPoint ) { origin.moveTo( aPoint.x(), aPoint.y() ); return *this; }
    virtual Node& moveBy( const double x, const double y ) { origin.moveBy( x, y ); return *this; }
    virtual Node& moveBy( const Point& aPoint ) { origin.moveBy( aPoint.x(), aPoint.y() ); return *this; }
    virtual Node& scaleBy( const double, const double );
    virtual Node& translateY( const double );
    Node& originMin( const double x, const double y ) { origin.min( x, y ); return *this; }
    Node& extentMax( const double w, const double h ) { extent.max( w, h ); return *this; }
    Node& resizeBox( const double x, const double y, const double w, const double h );

    virtual int direction() const { return 1; }
    virtual std::ostream& polygon( std::ostream&, const std::vector<Point>& points ) const = 0;
    virtual std::ostream& polyline( std::ostream&, const std::vector<Point>& points ) const = 0;
    virtual std::ostream& circle( std::ostream& output, const Point&, const double radius ) const = 0;
    virtual std::ostream& rectangle( std::ostream& output ) const = 0;
    virtual std::ostream& roundedRectangle( std::ostream& output ) const = 0;
    virtual std::ostream& text( std::ostream& output, const Point&, const char * ) const = 0;

    std::ostream& multi_server( std::ostream& output, const Point&, const double radius ) const;
    std::ostream& open_source( std::ostream& output, const Point&, const double ) const;
    std::ostream& open_sink( std::ostream& output, const Point&, const double ) const;
    std::ostream& draw_queue( std::ostream& output, const Point&, const double ) const;

protected:
    Point origin;
    Point extent;
};

#if EMF_OUTPUT
class NodeEMF : public Node, private EMF
{
private:
    NodeEMF(double x1, double y1, double x2, double y2 ) : Node(x1,y1,x2,y2) {}
public:
    static Node * create(double x1, double y1, double x2, double y2 ) { return new NodeEMF(x1,y1,x2,y2); }
    virtual std::ostream& polygon( std::ostream& output, const std::vector<Point>& points ) const;
    virtual std::ostream& polyline( std::ostream& output, const std::vector<Point>& points ) const;
    virtual std::ostream& circle( std::ostream& output, const Point& center, const double radius ) const;
    virtual std::ostream& rectangle( std::ostream& output ) const;
    virtual std::ostream& roundedRectangle( std::ostream& output ) const;
    virtual std::ostream& text( std::ostream& output, const Point&, const char * ) const;
    virtual std::ostream& comment( std::ostream& output, const std::string& ) const;
    virtual int direction() const { return -1; }
};
#endif

class NodeFig : public Node, protected Fig
{
protected:
    NodeFig(double x1, double y1, double x2, double y2 ) : Node(x1,y1,x2,y2) {}
public:
    static Node * create(double x1, double y1, double x2, double y2 ) { return new NodeFig(x1,y1,x2,y2); }
    virtual std::ostream& polygon( std::ostream&, const std::vector<Point>& points ) const;
    virtual std::ostream& polyline( std::ostream&, const std::vector<Point>& points ) const;
    virtual std::ostream& circle( std::ostream& output, const Point& center, const double radius  ) const;
    virtual std::ostream& rectangle( std::ostream& output ) const;
    virtual std::ostream& roundedRectangle( std::ostream& output ) const;
    virtual std::ostream& text( std::ostream& output, const Point&, const char * ) const;
    virtual std::ostream& comment( std::ostream& output, const std::string& ) const;
    virtual int direction() const { return -1; }
};

class NodePostScript : public Node, private PostScript
{
private:
    NodePostScript(double x1, double y1, double x2, double y2 ) : Node(x1,y1,x2,y2) {}
public:
    static Node * create(double x1, double y1, double x2, double y2 ) { return new NodePostScript(x1,y1,x2,y2); }
    virtual std::ostream& polygon( std::ostream&, const std::vector<Point>& points ) const;
    virtual std::ostream& polyline( std::ostream&, const std::vector<Point>& points ) const;
    virtual std::ostream& circle( std::ostream& output, const Point& center, const double radius  ) const;
    virtual std::ostream& rectangle( std::ostream& output ) const;
    virtual std::ostream& roundedRectangle( std::ostream& output ) const;
    virtual std::ostream& text( std::ostream& output, const Point&, const char * ) const;
    virtual std::ostream& comment( std::ostream& output, const std::string& ) const;
};

class NodeNull : public Node 
{
private:
    NodeNull(double x1, double y1, double x2, double y2 ) : Node(x1,y1,x2,y2) {}
public:
    static Node * create(double x1, double y1, double x2, double y2 ) { return new NodeNull(x1,y1,x2,y2); }
    virtual std::ostream& polygon( std::ostream& output, const std::vector<Point>& points ) const { return output; }
    virtual std::ostream& polyline( std::ostream& output, const std::vector<Point>& points ) const { return output; }
    virtual std::ostream& circle( std::ostream& output, const Point& center, const double radius  ) const { return output; }
    virtual std::ostream& rectangle( std::ostream& output ) const { return output; }
    virtual std::ostream& roundedRectangle( std::ostream& output ) const  { return output; }
    virtual std::ostream& text( std::ostream& output, const Point&, const char * ) const { return output; }
    virtual std::ostream& comment( std::ostream& output, const std::string& ) const { return output; }
};

class NodePsTeX : public NodeFig
{
private:
    NodePsTeX(double x1, double y1, double x2, double y2 ) : NodeFig(x1,y1,x2,y2) {}
public:
    static Node * create(double x1, double y1, double x2, double y2 ) { return new NodePsTeX(x1,y1,x2,y2); }
    virtual std::ostream& text( std::ostream& output, const Point&, const char * ) const;
};

#if SVG_OUTPUT
class NodeSVG : public Node, private SVG
{
private:
    NodeSVG(double x1, double y1, double x2, double y2 ) : Node(x1,y1,x2,y2) {}
public:
    static Node * create(double x1, double y1, double x2, double y2 ) { return new NodeSVG(x1,y1,x2,y2); }
    virtual std::ostream& polygon( std::ostream& output, const std::vector<Point>& points ) const;
    virtual std::ostream& polyline( std::ostream& output, const std::vector<Point>& points ) const;
    virtual std::ostream& circle( std::ostream& output, const Point& center, const double radius ) const;
    virtual std::ostream& rectangle( std::ostream& output ) const;
    virtual std::ostream& roundedRectangle( std::ostream& output ) const;
    virtual std::ostream& text( std::ostream& output, const Point&, const char * ) const;
    virtual std::ostream& comment( std::ostream& output, const std::string& ) const;
    virtual int direction() const { return -1; }
};
#endif

#if SXD_OUTPUT
class NodeSXD : public Node, private SXD
{
private:
    NodeSXD(double x1, double y1, double x2, double y2 ) : Node(x1,y1,x2,y2) {}
public:
    static Node * create(double x1, double y1, double x2, double y2 ) { return new NodeSXD(x1,y1,x2,y2); }
    virtual int direction() const { return -1; }
    virtual std::ostream& polygon( std::ostream& output, const std::vector<Point>& points ) const;
    virtual std::ostream& polyline( std::ostream& output, const std::vector<Point>& points ) const;
    virtual std::ostream& circle( std::ostream& output, const Point& center, const double radius ) const;
    virtual std::ostream& rectangle( std::ostream& output ) const;
    virtual std::ostream& roundedRectangle( std::ostream& output ) const;
    virtual std::ostream& text( std::ostream& output, const Point&, const char * ) const;
    virtual std::ostream& comment( std::ostream& output, const std::string& ) const;
};
#endif

class NodeTeX : public Node, private TeX
{
private:
    NodeTeX(double x1, double y1, double x2, double y2 ) : Node(x1,y1,x2,y2) {}
public:
    static Node * create(double x1, double y1, double x2, double y2 ) { return new NodeTeX(x1,y1,x2,y2); }
    virtual std::ostream& polygon( std::ostream&, const std::vector<Point>& points ) const;
    virtual std::ostream& polyline( std::ostream&, const std::vector<Point>& points ) const;
    virtual std::ostream& circle( std::ostream& output, const Point& center, const double radius  ) const;
    virtual std::ostream& rectangle( std::ostream& output ) const;
    virtual std::ostream& roundedRectangle( std::ostream& output ) const;
    virtual std::ostream& text( std::ostream& output, const Point&, const char * ) const;
    virtual std::ostream& comment( std::ostream& output, const std::string& ) const;
};

#if X11_OUTPUT
class NodeX11 : public Node, private X11
{
private:
    NodeX11(double x1, double y1, double x2, double y2 ) : Node(x1,y1,x2,y2) {}
public:
    static Node * create(double x1, double y1, double x2, double y2 ) { return new NodeX11(x1,y1,x2,y2); }
    virtual std::ostream& polygon( std::ostream& output, const std::vector<Point>& points ) const { return output; }
    virtual std::ostream& polyline( std::ostream& output, const std::vector<Point>& points ) const { return output; }
    virtual std::ostream& circle( std::ostream& output, const Point& center, const double radius  ) const{ return output; }
    virtual std::ostream& rectangle( std::ostream& output ) const;
    virtual std::ostream& roundedRectangle( std::ostream& output ) const;
    virtual std::ostream& text( std::ostream& output, const Point&, const char * ) const { return output; }
    virtual std::ostream& comment( std::ostream& output, const std::string& ) const;
};
#endif

#if HAVE_GD_H && HAVE_LIBGD
class NodeGD : public Node, private GD
{
private:
    NodeGD(double x1, double y1, double x2, double y2 ) : Node(x1,y1,x2,y2) {}
public:
    static Node * create(double x1, double y1, double x2, double y2 ) { return new NodeGD(x1,y1,x2,y2); }
    virtual std::ostream& polygon( std::ostream& output, const std::vector<Point>& points ) const;
    virtual std::ostream& polyline( std::ostream& output, const std::vector<Point>& points ) const;
    virtual std::ostream& circle( std::ostream& output, const Point& center, const double radius ) const;
    virtual std::ostream& rectangle( std::ostream& output ) const;
    virtual std::ostream& roundedRectangle( std::ostream& output ) const;
    virtual std::ostream& text( std::ostream& output, const Point&, const char * ) const;
    virtual std::ostream& comment( std::ostream& output, const std::string& ) const;
    virtual int direction() const { return -1; }
};
#endif
#endif
