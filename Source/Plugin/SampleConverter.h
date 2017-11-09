/*  Audio sample format converter
    Copyright (C) Lev Minkovsky
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#pragma once

struct SampleTypeData
{
   short container_width; //in bits
   short sample_width;    //in bits
   bool  isFloat;
   bool  bWrongType;

   SampleTypeData() 
   {
         container_width=0;
         sample_width=0 ; 
         isFloat=false; 
         bWrongType=true;
   }

   SampleTypeData(short _container_width,short _sample_width,bool _isFloat)
   {
         container_width=_container_width;
         sample_width=_sample_width ; 
         isFloat=_isFloat; 
         bWrongType= (container_width==0 || sample_width==0 || container_width<sample_width);
   }
};

//Auxiliary class to convert a single audio sample into a different format
class CSampleConverter
{
   const static double SCALE_FACTOR;
protected:
   SampleTypeData   m_inData;
   SampleTypeData   m_outData;
public:
   void convert(void * input,void * output) const;
   bool  isValid() const { return !(m_inData.bWrongType || m_outData.bWrongType);}
   short getInputContainerWidth() const  {return m_inData.container_width;}
   short getOutputContainerWidth() const {return m_outData.container_width;}
};

const double CSampleConverter::SCALE_FACTOR=((double)INT_MAX)+1.;

inline void CSampleConverter::convert(void * input,void * output) const
{
   if (m_inData.bWrongType || m_outData.bWrongType)
      return;
   //we will convert input into either int or double, depending on m_outData.isFloat
   int outInt=0; double outDouble=0;
   if (m_inData.isFloat)
   {
      double inDouble=(m_inData.sample_width==32)?(double)(*(float*)input):(*(double*)input);
      if (m_outData.isFloat)
         outDouble=inDouble;
      else
      {            
         double tmpDouble=floor(fabs(inDouble*SCALE_FACTOR)+0.5);
         if (inDouble<0)
            tmpDouble=-tmpDouble;
         if (tmpDouble>INT_MAX)
            tmpDouble=INT_MAX;
         if (tmpDouble<INT_MIN)
            tmpDouble=INT_MIN;
         outInt=(int)tmpDouble;
      }
   }
   else
   {
      //form an integer from the raw input memory
      int inInt=*(int *)input;
      /* We are in little-endian CPU so possible "junk" bits 
         are on the high end of the 32-bit word
         The next line removes them and brings the sample value to full 32-bit scale
      */
      inInt<<=(32-m_inData.container_width);
      /* Valid sample bits are left-aligned within the container, which itself
         is now left-aligned in the 32-bit word. 
	     We need to truncate(zero) all bits to the right of the sample bits 
       */
      int truncate = 32 - m_inData.sample_width;
      if (truncate > 0)
      {
         inInt>>=truncate;
         inInt<<=truncate;
      }
      //fill either outDouble or outInt depending on the output type	
      if (m_outData.isFloat)
         outDouble=((double)inInt)/SCALE_FACTOR;
      else
         outInt=inInt;
   }
   //possibly scale down and write to output
   if (m_outData.isFloat)
   {//outDouble has a valid value    
      if (m_outData.sample_width==32)
         *(float*)output=(float)outDouble;
      else                                            
         *(double*)output=outDouble;
   }
   else
   {//outInt has a valid value
      switch(m_outData.container_width)
      {
      case 32:
         *(int*)output=outInt;
         break;
      case 24:
         {
         struct target {int n:24; } ;
         ((target *)output)->n=outInt>>8;
         }
         break;
      case 16:
         {
         struct target {int n:16; } ;
         ((target *)output)->n=outInt>>16;
         }
         break;
      };
   }
};

