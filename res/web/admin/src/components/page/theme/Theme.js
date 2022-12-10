import {
    experimental_extendTheme as extendTheme
} from '@mui/material/styles';
import { blue, cyan, green, grey } from '@mui/material/colors';

export default function AppTheme(mode) {
    return extendTheme({
        colorSchemes:{
            dark:{
                palette:{
                    text:{
                        primary: green[200],
                        secondary: cyan[700]
                    }
                },
            },
            light: {
                palette:{
                    text:{
                        primary: grey[900],
                        secondary: blue[700]
                    }
                },
            }
        },
        typography: {
            subtitle1: {
                color: mode === 'dark' ? cyan[200] : cyan['900']
            },
            subtitle2: {
                color: mode === 'dark' ? cyan[100] : grey['600']
            },
            h6: {
                color: mode === 'dark' ? cyan['A100'] : cyan['50']
            },
            httpmethod: {
                color: mode === 'dark' ? cyan[100] : grey['600']
            },
            httppath: {
                color: mode === 'dark' ? cyan[400] : grey['900']
            }
        },
        components: {
            MuiSvgIcon: {
              styleOverrides: {
                root: {
                    color: mode === "dark" ? green["A400"] : 'black'
                },
              },
            },
            MuiListItemText: {
                styleOverrides: {
                  root: {
                      textTransform: 'capitalize',
                  },
                },
            },
            main: {
                styleOverrides: {
                    root: {
                        with: "100%"
                    }
                }
            }
        }
    });
}