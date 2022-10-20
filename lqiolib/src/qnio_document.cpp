/* -*- c++ -*-
 * $Id: qnio_document.cpp 15969 2022-10-13 19:49:43Z greg $
 *
 * Superclass for Queueing Network models.
 *
 * Copyright the Real-Time and Distributed Systems Group,
 * Department of Systems and Computer Engineering,
 * Carleton University, Ottawa, Ontario, Canada. K1S 5B6
 *
 * ------------------------------------------------------------------------
 * September 2022
 * ------------------------------------------------------------------------
 */

#include "qnio_document.h"
#include "dom_document.h"

QNIO::Document::Document( const std::string& input_file_name, const BCMP::Model& model )
    : _input_file_name(input_file_name), _pragmas(), _model(model), _lqx_program(nullptr)
{
    LQIO::DOM::Document::__input_file_name = input_file_name;
}

QNIO::Document::~Document()
{
    LQIO::DOM::Document::__input_file_name.clear();
}
