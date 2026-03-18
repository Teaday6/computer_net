#pragma once

namespace FrameConfig {
    const int IMAGE_WIDTH = 512;
    const int IMAGE_HEIGHT = 512;

    // 【物理外挂开启】：将像素块放大一倍！面积扩大4倍！
    const int CELL_SIZE = 16;

    const int GRID_COLS = (IMAGE_WIDTH / CELL_SIZE);
    const int GRID_ROWS = (IMAGE_HEIGHT / CELL_SIZE);
    const int BITS_PER_FRAME = GRID_COLS * GRID_ROWS;
    const int FRAME_NUMBER_SIZE = 2;
    const int CRC_SIZE = 2;

    // 标记占据 9x9 的格子，包含外围的一圈白色保护带
    const int MARKER_BLOCK = 9;
    const int MARKER_CELLS = 4 * (MARKER_BLOCK * MARKER_BLOCK);

    // 动态计算数据容量
    const int DATA_PAYLOAD_SIZE = BITS_PER_FRAME - (FRAME_NUMBER_SIZE * 8) - (CRC_SIZE * 8) - MARKER_CELLS;
    const int MAX_FRAMES = 65536;
}
