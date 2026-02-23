/**
 * Symbol selector dropdown component
 */
import { Select } from '@/shared/components';
import { useMarketStore } from '../store';

export function SymbolSelector() {
  const selectedSymbol = useMarketStore((state) => state.selectedSymbol);
  const availableSymbols = useMarketStore((state) => state.availableSymbols);
  const setSelectedSymbol = useMarketStore((state) => state.setSelectedSymbol);

  const options = availableSymbols.map((symbol) => ({
    value: symbol,
    label: symbol,
  }));

  return (
    <Select
      label="Symbol"
      options={options}
      value={selectedSymbol}
      onChange={(e) => setSelectedSymbol(e.target.value)}
      fullWidth={false}
      className="w-40"
    />
  );
}
