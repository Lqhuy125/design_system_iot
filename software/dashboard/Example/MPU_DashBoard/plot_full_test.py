from pymongo import MongoClient
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt

# ====== CONFIG ======
MONGO_URI = "mongodb://localhost:27017"
DB_NAME   = "bridge_shm"
COLL_NAME = "imu_samples"

# Lựa chọn node để thực hiện vẽ đồ thị
AREA = "1"
NODE = "N02"

# Lựa chọn khoảng dài thời gian để vẽ đồ thị bắt đầu từ start time bên dưới
DURATION_S = 120

# Start time cho từng test (anh điền đúng 3 mốc)
TEST_STARTS = {
    1: "2026-01-31T09:09:49.925Z",   # test1: verify Z ~ -1
    2: "2026-01-31T09:25:08.973Z",   # test2: verify Y ~ -1
    3: "2026-01-31T09:29:10.229Z",   # test3: verify X ~ -1
}
# ====================

def load_test_window(col, start_iso, duration_s, area, node):
    start = pd.Timestamp(start_iso)
    end   = start + pd.Timedelta(seconds=duration_s)

    query = {
        "area": area,
        "name_node": node,
        "time": {"$gte": start.to_pydatetime(), "$lt": end.to_pydatetime()}
    }
    projection = {"_id": 0, "time": 1, "ax": 1, "ay": 1, "az": 1}

    docs = list(col.find(query, projection).sort("time", 1))
    df = pd.DataFrame(docs)
    if df.empty:
        return df

    df["time"] = pd.to_datetime(df["time"], utc=True)
    df = df.dropna(subset=["time"]).sort_values("time").reset_index(drop=True)

    # t[s] từ sample đầu tiên trong window thực tế
    t0 = df["time"].iloc[0]
    df["t"] = (df["time"] - t0).dt.total_seconds()

    # giữ trong [0, duration]
    df = df[(df["t"] >= 0) & (df["t"] <= duration_s)].copy()
    return df

def build_all_tests_dataframe():
    client = MongoClient(MONGO_URI)
    col = client[DB_NAME][COLL_NAME]

    frames = []
    missing = []

    for test_id, start_iso in TEST_STARTS.items():
        df = load_test_window(col, start_iso, DURATION_S, AREA, NODE)
        if df.empty:
            missing.append(test_id)
            continue
        df["test"] = test_id
        frames.append(df)

    if missing:
        print(f"⚠️ Không có dữ liệu cho test: {missing}. "
              f"Hãy kiểm tra START time/AREA/NODE hoặc window 120s.")

    if not frames:
        raise ValueError("Không load được dữ liệu test nào. Dừng.")

    all_df = pd.concat(frames, ignore_index=True)
    return all_df

def to_long_format(df):
    long_df = df.melt(
        id_vars=["t", "test"],
        value_vars=["ax", "ay", "az"],
        var_name="axis",
        value_name="a_g"
    )
    long_df["axis"] = long_df["axis"].str.replace("a", "", regex=False)  # ax->x...
    return long_df

def plot_3x3_like_paper(long_df, fixed_limits=True):
    # --- style giống paper ---
    plt.rcParams.update({
        "font.family": "serif",
        "font.size": 9,
        "axes.titlesize": 9,
        "axes.labelsize": 9,
        "xtick.labelsize": 8,
        "ytick.labelsize": 8,
        "axes.linewidth": 0.8,
    })

    tests = [1, 2, 3]
    axes_order = ["x", "y", "z"]
    letters = list("abcdefghi")

    xlim = (0, 120)
    xticks = np.arange(0, 121, 30)

    # Test expectation: test1 -> z ~ -1, test2 -> y ~ -1, test3 -> x ~ -1
    expected_minus1 = {1: "z", 2: "y", 3: "x"}

    # y-limits "verify-friendly" (anh có thể chỉnh)
    ylim_zero = (-0.10, 0.10)
    ylim_minus1 = (-1.10, -0.90)

    def auto_ylim(y, pad_ratio=0.15, min_pad=1e-4):
        y = np.asarray(y, dtype=float)
        ymin, ymax = np.nanmin(y), np.nanmax(y)
        span = max(ymax - ymin, min_pad)
        pad = span * pad_ratio
        return (ymin - pad, ymax + pad)

    fig, axs = plt.subplots(3, 3, figsize=(7.2, 6.2), sharex=False, sharey=False)
    fig.subplots_adjust(wspace=0.45, hspace=0.75)

    k = 0
    for r, test_id in enumerate(tests):
        for c, axis in enumerate(axes_order):
            ax = axs[r, c]
            sub = long_df[(long_df["test"] == test_id) & (long_df["axis"].str.lower() == axis)].copy()
            sub = sub.sort_values("t")
            y = sub["a_g"].astype(float)

            ax.plot(sub["t"], y, color="black", linewidth=0.6)

            # Title giống paper
            ax.set_title(f"TH{axis}_test{test_id}")
            ax.set_xlabel("t[s]")
            ax.set_ylabel("a[g]")

            ax.set_xlim(*xlim)
            ax.set_xticks(xticks)

            # y-limits
            if fixed_limits:
                if axis == expected_minus1.get(test_id):
                    ax.set_ylim(*ylim_minus1)  # trục verify -1g
                else:
                    ax.set_ylim(*ylim_zero)    # trục quanh 0
            else:
                ax.set_ylim(*auto_ylim(y))

            ax.tick_params(direction="out", length=3, width=0.8)
            ax.text(0.5, -0.38, f"({letters[k]})",
                    transform=ax.transAxes, ha="center", va="top")
            k += 1

    return fig

def print_quick_stats(all_df):
    # thống kê nhanh cho từng test để verify định lượng
    for test_id in sorted(all_df["test"].unique()):
        df_t = all_df[all_df["test"] == test_id]
        stats = df_t[["ax","ay","az"]].agg(["mean","std","min","max"])
        print(f"\n=== Test {test_id} stats ===")
        print(stats)

# ====== RUN ======
all_df = build_all_tests_dataframe()
print_quick_stats(all_df)

long_df = to_long_format(all_df)
fig = plot_3x3_like_paper(long_df, fixed_limits=True)  # fixed_limits=False nếu muốn auto
plt.show()

# Lưu hình nếu cần:
# fig.savefig("imu_3tests_3x3.png", dpi=300, bbox_inches="tight")
# fig.savefig("imu_3tests_3x3.pdf", bbox_inches="tight")