import numpy as np
import pandas as pd


#generate random walk for price simulation
#repeat if negative prices are generated
rand_walk = np.zeros(1000000) 
while(rand_walk.min() < 10):
    rand_walk = np.random.randint(low=-1, high=2, size=1000000)
    rand_walk[0] = 10000
    rand_walk = np.cumsum(rand_walk)
    
#make pandas series for all columns
order_type_series = pd.Series(np.random.randint(3, size=1000000))
order_volume_series = pd.Series(np.random.randint(low=-3000, high=3000, size=1000000))
order_price_series_erratic = pd.Series(np.random.randint(low=7000, high=13000, size=1000000))
order_price_series_rw = pd.Series(rand_walk)

order_price_series_rw.plot()

#make dictionaries of series for creating dataframes
frame_dict_erratic = {
    'order_type': order_type_series,
    'order_volume': order_volume_series,
    'order_price': order_price_series_erratic
    }
frame_dict_rw = {
    'order_type': order_type_series,
    'order_volume': order_volume_series,
    'order_price': order_price_series_rw
    }

#make dataframes from dictionaries
csv_frame_erratic = pd.DataFrame(frame_dict_erratic)
csv_frame_rw = pd.DataFrame(frame_dict_rw)

#generate CSVs
csv_frame_erratic.to_csv('RandTestDataErratic.csv', header=False, index=False)
csv_frame_rw.to_csv('RandTestDataRW.csv', header=False, index=False)