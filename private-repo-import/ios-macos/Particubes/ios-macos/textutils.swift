//
//  textutils.swift
//  Blip
//
//  Created by Adrien Duermael on 17/04/2024.
//

import Foundation

// Returns String with grapheme cluster aware range.
func utf8PointerAndByteRangeToStringAndRange(utf8Data: Data, start: Int, end: Int) -> (string: String, start: Int, end: Int)? {
    // Convert UnsafePointer<CChar> to String
    guard let string = String(data: utf8Data, encoding: .utf8) else {
        return nil
    }

    var startIndex = string.endIndex
    var endIndex = string.endIndex
    var currentByteIndex = 0

    // Find the start and end indices within the String by iterating over its indices
    for index in string.indices {
        if currentByteIndex >= start { // >= because it may jump beyond in some cases
            startIndex = index
            break
        }

        let charData = String(string[index]).data(using: .utf8)!
        currentByteIndex += charData.count
    }

    for index in string.indices[startIndex...] {
        if currentByteIndex >= end { // >= because it may jump beyond in some cases
            endIndex = index
            break
        }

        let charData = String(string[index]).data(using: .utf8)!
        currentByteIndex += charData.count
    }

    // Convert String.Index to integer offsets
    let startOffset = string.distance(from: string.startIndex, to: startIndex)
    let endOffset = string.distance(from: string.startIndex, to: endIndex)

    return (string, startOffset, endOffset)
}

func stringAndRangeToUtf8DataAndByteRange(string: String, start: Int, end: Int) -> (data: Data, start: Int, end: Int)? {
    let utf8Data = Data(string.utf8)

    guard let startIndex = string.index(string.startIndex, offsetBy: start, limitedBy: string.endIndex),
          let endIndex = string.index(string.startIndex, offsetBy: end, limitedBy: string.endIndex),
          startIndex <= endIndex else {
        return nil
    }

    var startByteIndex = 0
    var endByteIndex = 0

    // Calculate the byte index for the start index
    var currentIndex = string.startIndex
    while currentIndex < startIndex {
        let nextIndex = string.index(after: currentIndex)
        let charData = String(string[currentIndex]).data(using: .utf8)!
        startByteIndex += charData.count
        currentIndex = nextIndex
    }

    // Calculate the byte index for the end index
    endByteIndex = startByteIndex  // Start counting endByteIndex from startByteIndex
    while currentIndex < endIndex {
        let nextIndex = string.index(after: currentIndex)
        let charData = String(string[currentIndex]).data(using: .utf8)!
        endByteIndex += charData.count
        currentIndex = nextIndex
    }

    return (utf8Data, startByteIndex, endByteIndex)
}
