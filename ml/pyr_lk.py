import torch
import torch.nn as nn
import torch.nn.functional as F
import math


class PyramidalLK(nn.Module):
    def __init__(self, window_size: int = 5, max_levels: int | None = None, eps: float = 1e-6):
        super().__init__()
        self.window_size = window_size
        self.max_levels = max_levels
        self.eps = eps

        # Sobel filters
        sobel_x = torch.tensor([[-1., 0., 1.],
                                [-2., 0., 2.],
                                [-1., 0., 1.]]).view(1, 1, 3, 3) / 8.0
        sobel_y = sobel_x.transpose(2, 3)
        self.register_buffer('sobel_x', sobel_x)
        self.register_buffer('sobel_y', sobel_y)

        # box filter for window sums
        box = torch.ones(1, 1, window_size, window_size)
        self.register_buffer('box', box)

    def build_pyramid(self, img: torch.Tensor, levels: int) -> list[torch.Tensor]:
        pyr = [img]
        for _ in range(1, levels):
            pyr.append(F.avg_pool2d(pyr[-1], 2, 2))
        return pyr

    def warp(self, img: torch.Tensor, flow: torch.Tensor) -> torch.Tensor:
        B, C, H, W = img.shape
        ys, xs = torch.meshgrid(torch.arange(H, device=img.device),
                                torch.arange(W, device=img.device),
                                indexing='ij')
        grid = torch.stack((xs, ys), dim=0).unsqueeze(0).repeat(B, 1, 1, 1).float()
        coords = grid + flow
        coords[:, 0] = 2 * coords[:, 0] / (W - 1) - 1
        coords[:, 1] = 2 * coords[:, 1] / (H - 1) - 1
        return F.grid_sample(img, coords.permute(0, 2, 3, 1), align_corners=True)

    def lucas_kanade(self, I1: torch.Tensor, I2: torch.Tensor) -> torch.Tensor:
        # To grayscale
        if I1.shape[1] > 1:
            I1 = I1.mean(1, keepdim=True)
            I2 = I2.mean(1, keepdim=True)

        # Spatial gradients
        Ix = F.conv2d(I1, self.sobel_x, padding=1)
        Iy = F.conv2d(I1, self.sobel_y, padding=1)
        It = I2 - I1

        Ixx = Ix * Ix;
        Iyy = Iy * Iy;
        Ixy = Ix * Iy
        Ixt = Ix * It;
        Iyt = Iy * It

        # Asymmetric padding for even window sizes
        w = self.window_size
        pad_h = (w // 2, w - w // 2 - 1)
        pad_w = (w // 2, w - w // 2 - 1)
        # Pad order: (left, right, top, bottom)
        pad = (pad_w[0], pad_w[1], pad_h[0], pad_h[1])

        # Sum within window
        Sxx = F.conv2d(F.pad(Ixx, pad), self.box)
        Syy = F.conv2d(F.pad(Iyy, pad), self.box)
        Sxy = F.conv2d(F.pad(Ixy, pad), self.box)
        Sxt = F.conv2d(F.pad(Ixt, pad), self.box)
        Syt = F.conv2d(F.pad(Iyt, pad), self.box)

        # Solve 2x2 system
        det = Sxx * Syy - Sxy * Sxy
        u = (-Syy * Sxt + Sxy * Syt) / (det + self.eps)
        v = (Sxy * Sxt - Sxx * Syt) / (det + self.eps)
        return torch.cat([u, v], dim=1)

    def forward(self, img1: torch.Tensor, img2: torch.Tensor) -> torch.Tensor:
        B, C, H, W = img1.shape

        if self.max_levels is None:
            min_sz = min(H, W)
            levels = max(1, int(math.floor(math.log2(min_sz / self.window_size))) + 1)
        else:
            levels = max(1, self.max_levels)

        pyr1 = self.build_pyramid(img1, levels)
        pyr2 = self.build_pyramid(img2, levels)

        # Init flow at coarsest
        flow = torch.zeros(B, 2, pyr1[-1].shape[2], pyr1[-1].shape[3], device=img1.device)
        for lvl in range(levels - 1, -1, -1):
            I1, I2 = pyr1[lvl], pyr2[lvl]
            if lvl < levels - 1:
                flow = F.interpolate(flow, size=I1.shape[2:], mode='bilinear', align_corners=True) * 2
            I2w = self.warp(I2, flow)
            duv = self.lucas_kanade(I1, I2w)
            flow = flow + duv

        # Upsample to original resolution if needed
        if flow.shape[2:] != (H, W):
            flow = F.interpolate(flow, size=(H, W), mode='bilinear', align_corners=True)
            flow[:, 0] *= W / flow.shape[3]
            flow[:, 1] *= H / flow.shape[2]
        return flow
