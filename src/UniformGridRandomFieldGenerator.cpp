/*

Copyright (c) 2005-2017, University of Oxford.
All rights reserved.

University of Oxford means the Chancellor, Masters and Scholars of the
University of Oxford, having an administrative office at Wellington
Square, Oxford OX1 2JD, UK.

This file is part of Chaste.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
 * Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.
 * Neither the name of the University of Oxford nor the names of its
   contributors may be used to endorse or promote products derived from this
   software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#include <iomanip>
#include <fstream>
#include <numeric>

#include <eigen3/Eigen/Dense>

#include "Exception.hpp"
#include "FileFinder.hpp"
#include "UniformGridRandomFieldGenerator.hpp"

#include "Debug.hpp"

template <unsigned SPACE_DIM>
UniformGridRandomFieldGenerator<SPACE_DIM>::UniformGridRandomFieldGenerator(std::array<double, SPACE_DIM> lowerCorner,
                                                                            std::array<double, SPACE_DIM> upperCorner,
                                                                            std::array<unsigned, SPACE_DIM> numGridPts,
                                                                            std::array<bool, SPACE_DIM> periodicity,
                                                                            unsigned numEigenvals,
                                                                            double lengthScale)
        : mLowerCorner(lowerCorner),
          mUpperCorner(upperCorner),
          mNumGridPts(numGridPts),
          mPeriodicity(periodicity),
          mNumEigenvals(numEigenvals),
          mLengthScale(lengthScale)
{
    // First check if there's a cached random field matching these parameters
    FileFinder cached_version_file(GetFilenameFromParams(),RelativeTo::ChasteTestOutput);

    if (cached_version_file.Exists())
    {
        LoadFromCache(cached_version_file.GetAbsolutePath());
    }
    else
    {

    }
}

template <unsigned SPACE_DIM>
double UniformGridRandomFieldGenerator<SPACE_DIM>::GetSquaredDistAtoB(
    const c_vector<double, SPACE_DIM>& rLocation1,
    const c_vector<double, SPACE_DIM>& rLocation2) const noexcept
{
    double dist_squared = 0.0;

    // Loop over dimensions
    for (unsigned dim = 0; dim < SPACE_DIM; ++dim)
    {
        // Get the (non-periodic) absolute difference
        double delta_this_dim = std::fabs(rLocation2[dim] - rLocation1[dim]);

        // If this dimension is periodic, recalculate the distance if necessary
        if (mPeriodicity[dim])
        {
            const double domain_width_this_dim = mUpperCorner[dim] - mLowerCorner[dim];

            delta_this_dim = std::min(delta_this_dim, domain_width_this_dim - delta_this_dim);
        }

        dist_squared += delta_this_dim * delta_this_dim;
    }

    return dist_squared;
}

template <unsigned SPACE_DIM>
std::string UniformGridRandomFieldGenerator<SPACE_DIM>::GetFilenameFromParams() const noexcept
{
    std::stringstream name;
    name << std::fixed << std::setprecision(3) << "CachedRandomFields/";

    switch(SPACE_DIM)
    {
        case 1:
        {
            name << "x_"
                 << mLowerCorner[0] << "_"
                 << mUpperCorner[0] << "_"
                 << mNumGridPts[0] << "_"
                 << mPeriodicity[0] << "_"
                 << mNumEigenvals << "_"
                 << mLengthScale;
            break;
        }
        case 2:
        {
            name << "xy_"
                 << mLowerCorner[0] << "_" << mLowerCorner[1] << "_"
                 << mUpperCorner[0] << "_" << mUpperCorner[1] << "_"
                 << mNumGridPts[0] << "_" << mNumGridPts[1] << "_"
                 << mPeriodicity[0] << "_" << mPeriodicity[1] << "_"
                 << mNumEigenvals << "_"
                 << mLengthScale;
            break;
        }
        case 3:
        {
            name << "xyz_"
                 << mLowerCorner[0] << "_" << mLowerCorner[1] << "_" << mLowerCorner[2] << "_"
                 << mUpperCorner[0] << "_" << mUpperCorner[1] << "_" << mUpperCorner[2] << "_"
                 << mNumGridPts[0] << "_" << mNumGridPts[1] << "_" << mNumGridPts[2] << "_"
                 << mPeriodicity[0] << "_" << mPeriodicity[1] << "_" << mPeriodicity[2] << "_"
                 << mNumEigenvals << "_"
                 << mLengthScale;
            break;
        }
        default:
            NEVER_REACHED;
    }

    // Add file extension
    name << ".rfg";
    return name.str();
}

template <unsigned SPACE_DIM>
void UniformGridRandomFieldGenerator<SPACE_DIM>::LoadFromCache(const std::string& absoluteFilePath)
{
    RandomFieldCacheHeader<SPACE_DIM> header;

    std::ifstream input_file(absoluteFilePath, std::ios::in | std::ios::binary);
    EXCEPT_IF_NOT(input_file.is_open());

    // Read the header, and populate class parameters
    input_file.read((char*) &header, sizeof(RandomFieldCacheHeader<SPACE_DIM>));
    mLowerCorner = header.mLowerCorner;
    mUpperCorner = header.mUpperCorner;
    mNumGridPts = header.mNumGridPts;
    mPeriodicity = header.mPeriodicity;
    mNumEigenvals = header.mNumEigenvals;
    mLengthScale = header.mLengthScale;

    // Calculate how many grid points there are in total, and resize the eigen data arrays accordingly
    const unsigned total_gridpts = std::accumulate(mNumGridPts.begin(), mNumGridPts.end(), 1u, std::multiplies<unsigned>());
    mEigenvals.resize(mNumEigenvals);
    mEigenvecs.resize(total_gridpts, mNumEigenvals);

    // Read the eigenvalues and eigenvectors into their respective data arrays
    input_file.read((char*) mEigenvals.data(), mNumEigenvals * sizeof(double));
    input_file.read((char*) mEigenvecs.data(), total_gridpts * mNumEigenvals * sizeof(double));

    input_file.close();
}

template <unsigned SPACE_DIM>
void UniformGridRandomFieldGenerator<SPACE_DIM>::SaveToCache()
{
    // Get the absolute file path to the cached file, given the current parameters
    FileFinder cached_version_file(GetFilenameFromParams(), RelativeTo::ChasteTestOutput);
    std::ofstream output_file(cached_version_file.GetAbsolutePath(), std::ios::out | std::ios::binary);
    EXCEPT_IF_NOT(output_file.is_open());

    // Generate the header struct
    RandomFieldCacheHeader<SPACE_DIM> header;
    header.mLowerCorner = mLowerCorner;
    header.mUpperCorner = mUpperCorner;
    header.mNumGridPts = mNumGridPts;
    header.mPeriodicity = mPeriodicity;
    header.mNumEigenvals = mNumEigenvals;
    header.mLengthScale = mLengthScale;

    // Write the information to file
    output_file.write((char*) &header, sizeof(RandomFieldCacheHeader<SPACE_DIM>));
    output_file.write((char*) mEigenvals.data(), mEigenvals.size() * sizeof(double));
    output_file.write((char*) mEigenvecs.data(), mEigenvecs.size() * sizeof(double));
    output_file.close();
}

// Explicit instantiation
template class UniformGridRandomFieldGenerator<1>;
template class UniformGridRandomFieldGenerator<2>;
template class UniformGridRandomFieldGenerator<3>;
