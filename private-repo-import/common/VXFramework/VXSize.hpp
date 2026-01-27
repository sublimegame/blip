//
//  VXSize.hpp
//  Cubzh
//
//  Created by Gaetan de Villele on 10/02/2020.
//  Copyright © 2020 voxowl. All rights reserved.
//

#pragma once

// engine
#include "utils.h"
#include "config.h"

namespace vx {

class Size final {
    
public:
        
    /**
     * Constructor
     */
    Size() :
    _width(0.0f),
    _height(0.0f)
    {
    }
    
    /**
     * Destructor
     */
    ~Size()
    {}
    
    /**
     *
     */
    Size( const float width, const float height ) :
    _width( width ),
    _height( height )
    {}
    
    /**
     *
     */
    Size( const Size& size ) :
    _width( size._width ),
    _height( size._height )
    {}
    
    /**
     *
     */
    Size& operator=( const Size& size )
    {
        _width = size._width;
        _height = size._height;
        
        return *this;
    }
    
    Size& operator*=( const float& coef )
    {
        _width *= coef;
        _height *= coef;
        
        return *this;
    }
    
    bool operator==(const Size& s) const {
        return (float_isEqual(this->_width, s._width, EPSILON_0_0001_F) && float_isEqual(this->_height, s._height, EPSILON_0_0001_F));
    }
    
    bool operator!=(const Size& s) const {
        return (*this == s) == false;
    }
    
    
#pragma mark Accessors
    
    inline float getWidth() const          { return _width; }
    inline void setWidth( float width )    { _width = width; }
    inline float getHeight() const         { return _height; }
    inline void setHeight( float height )  { _height = height; }
    inline void set( float width, float height )  { _width = width; _height = height; }
    
    
private:
    /** in points */
    float _width;
    
    /** in points */
    float _height;
    
};

}
